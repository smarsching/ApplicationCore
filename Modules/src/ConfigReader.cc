// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ConfigReader.h"

#include "VariableGroup.h"
#include <libxml++/libxml++.h>

#include <iostream>

namespace ChimeraTK {

  template<typename Element>
  static Element prefix(std::string s, Element e) {
    e.name = s + e.name;
    return e;
  }

  static std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& fileName);
  static std::string root(std::string flattened_name);
  static std::string branchWithoutRoot(std::string flattened_name);
  static std::string branch(std::string flattened_name);
  static std::string leaf(std::string flattened_name);

  struct Variable {
    std::string name;
    std::string type;
    std::string value;
  };

  struct Array {
    std::string name;
    std::string type;
    std::map<size_t, std::string> values;
  };

  using VariableList = std::vector<Variable>;
  using ArrayList = std::vector<Array>;

  class ConfigParser {
    std::string fileName_{};
    std::unique_ptr<xmlpp::DomParser> parser_{};
    std::unique_ptr<VariableList> variableList_{};
    std::unique_ptr<ArrayList> arrayList_{};

   public:
    ConfigParser(const std::string& fileName) : fileName_(fileName), parser_(createDomParser(fileName)) {}

    std::unique_ptr<VariableList> getVariableList();
    std::unique_ptr<ArrayList> getArrayList();

   private:
    std::tuple<std::unique_ptr<VariableList>, std::unique_ptr<ArrayList>> parse();
    xmlpp::Element* getRootNode(xmlpp::DomParser& parser);
    void error(const std::string& message);
    bool isVariable(const xmlpp::Element* element);
    bool isArray(const xmlpp::Element* element);
    bool isModule(const xmlpp::Element* element);
    Variable parseVariable(const xmlpp::Element* element);
    Array parseArray(const xmlpp::Element* element);
    void parseModule(const xmlpp::Element* element, std::string parent_name);

    void validateValueNode(const xmlpp::Element* valueElement);
    std::map<size_t, std::string> gettArrayValues(const xmlpp::Element* element);
  };

  class ModuleTree : public VariableGroup {
   public:
    // Note: This has hideThis as default modifier, because we want the level of
    // the ModuleTree to vanish in its owner.
    ModuleTree(EntityOwner* owner, std::string name, std::string description,
        HierarchyModifier modifier = HierarchyModifier::hideThis)
    : VariableGroup{owner, name, description, modifier} {}

    ChimeraTK::Module* lookup(std::string flattened_module_name);

   private:
    void addChildNode(std::string name) {
      if(_children.find(name) == _children.end()) {
        _children[name] = std::make_unique<ModuleTree>(this, name, "", HierarchyModifier::none);
      }
    }

    ChimeraTK::ModuleTree* get(std::string flattened_name);

    std::unordered_map<std::string, std::unique_ptr<ModuleTree>> _children;
  };

  /*********************************************************************************************************************/

  /** Functor to fill variableMap */
  struct FunctorFill {
    FunctorFill(ConfigReader* owner, const std::string& type, const std::string& name, const std::string& value,
        bool& processed)
    : _owner(owner), _type(type), _name(name), _value(value), _processed(processed) {
      _processed = false;
    }

    template<typename PAIR>
    void operator()(PAIR&) const {
      // extract the user type from the pair
      typedef typename PAIR::first_type T;

      // skip this type, if not matching the type string in the config file
      if(_type != boost::fusion::at_key<T>(_owner->typeMap)) return;

      _owner->createVar<T>(_name, _value);
      _processed = true;
    }

    ConfigReader* _owner;
    const std::string &_type, &_name, &_value;
    bool& _processed; // must be a non-const reference, since we want to return
                      // this to the caller
  };

  /*********************************************************************************************************************/

  /** Functor to fill variableMap for arrays */
  struct ArrayFunctorFill {
    ArrayFunctorFill(ConfigReader* owner, const std::string& type, const std::string& name,
        const std::map<size_t, std::string>& values, bool& processed)
    : _owner(owner), _type(type), _name(name), _values(values), _processed(processed) {
      _processed = false;
    }

    template<typename PAIR>
    void operator()(PAIR&) const {
      // extract the user type from the pair
      typedef typename PAIR::first_type T;

      // skip this type, if not matching the type string in the config file
      if(_type != boost::fusion::at_key<T>(_owner->typeMap)) return;

      _owner->createArray<T>(_name, _values);
      _processed = true;
    }

    ConfigReader* _owner;
    const std::string &_type, &_name;
    const std::map<size_t, std::string>& _values;
    bool& _processed; // must be a non-const reference, since we want to return
                      // this to the caller
  };

  /*********************************************************************************************************************/

  struct FunctorGetTypeForName {
    FunctorGetTypeForName(const ConfigReader* owner, std::string const& name, std::string& type)
    : _owner(owner), _name(name), _type(type) {}

    template<typename PAIR>
    bool operator()(PAIR const& pair) const {
      // extract the user type from the pair
      typedef typename PAIR::first_type T;

      size_t numberOfMatches{pair.second.count(_name)};
      assert(numberOfMatches <= 1);
      bool hasMatch{numberOfMatches == 1};

      if(hasMatch) {
        _type = boost::fusion::at_key<T>(_owner->typeMap);
      }

      return hasMatch;
    }

    const ConfigReader* _owner;
    const std::string& _name;
    std::string& _type;
  };

  /*********************************************************************************************************************/

  void ConfigReader::checkVariable(std::string const& name, std::string const& typeOfThis) const {
    std::string typeOfVar{""};

    bool varExists = boost::fusion::any(variableMap.table, FunctorGetTypeForName{this, name, typeOfVar});

    if(!varExists) {
      auto msg = "ConfigReader: Cannot find a scalar configuration variable of the name '" + name +
          "' in the config file '" + _fileName + "'.";
      std::cerr << msg << std::endl;
      throw(ChimeraTK::logic_error(msg));
    }

    if(typeOfVar != typeOfThis) {
      auto msg = "ConfigReader: Attempting to read scalar configuration variable '" + name + "' with type '" +
          typeOfThis + "'. This does not match type '" + typeOfVar + "' defined in the config file.";
      std::cerr << msg << std::endl;
      throw(ChimeraTK::logic_error(msg));
    }
  }

  /*********************************************************************************************************************/

  void ConfigReader::checkArray(std::string const& name, std::string const& typeOfThis) const {
    std::string typeOfVar{""};

    bool varExists = boost::fusion::any(arrayMap.table, FunctorGetTypeForName{this, name, typeOfVar});

    if(!varExists) {
      throw(ChimeraTK::logic_error("ConfigReader: Cannot find a array "
                                   "configuration variable of the name '" +
          name + "' in the config file '" + _fileName + "'."));
    }

    if(typeOfVar != typeOfThis) {
      throw(ChimeraTK::logic_error("ConfigReader: Attempting to read array configuration variable '" + name +
          "' with type '" + typeOfThis + "'. This does not match type '" + typeOfVar +
          "' defined in the config file."));
    }
  }

  /*********************************************************************************************************************/

  template<typename T>
  void ConfigReader::createVar(const std::string& name, const std::string& value) {
    T convertedValue = ChimeraTK::userTypeToUserType<T>(value);

    auto moduleName = branch(name);
    auto varName = leaf(name);
    auto varOwner = _moduleTree->lookup(moduleName);

    // place the variable onto the vector
    std::unordered_map<std::string, ConfigReader::Var<T>>& theMap = boost::fusion::at_key<T>(variableMap.table);
    theMap.emplace(std::make_pair(name, ConfigReader::Var<T>(varOwner, varName, convertedValue)));
  }

  /*********************************************************************************************************************/

  /*********************************************************************************************************************/

  template<typename T>
  void ConfigReader::createArray(const std::string& name, const std::map<size_t, std::string>& values) {
    std::vector<T> Tvalues;

    size_t expectedIndex = 0;
    for(auto it = values.begin(); it != values.end(); ++it) {
      // check index (std::map should be ordered by the index)
      if(it->first != expectedIndex) {
        parsingError("Array index " + std::to_string(expectedIndex) + " not found, but " + std::to_string(it->first) +
            " was. "
            "Sparse arrays are not supported!");
      }
      ++expectedIndex;

      // convert value into user type
      T convertedValue = ChimeraTK::userTypeToUserType<T>(it->second);

      // store value in vector
      Tvalues.push_back(convertedValue);
    }

    auto moduleName = branch(name);
    auto arrayName = leaf(name);
    auto arrayOwner = _moduleTree->lookup(moduleName);

    // place the variable onto the vector
    std::unordered_map<std::string, ConfigReader::Array<T>>& theMap = boost::fusion::at_key<T>(arrayMap.table);
    theMap.emplace(std::make_pair(name, ConfigReader::Array<T>(arrayOwner, arrayName, Tvalues)));
  }

  /*********************************************************************************************************************/

  ConfigReader::ConfigReader(EntityOwner* owner, const std::string& name, const std::string& fileName,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ApplicationModule(owner, name, "Configuration read from file '" + fileName + "'", hierarchyModifier, tags),
    _fileName(fileName), _moduleTree(std::make_unique<ModuleTree>(this, name + "-ModuleTree", "")) {
    construct(fileName);
  }

  /*********************************************************************************************************************/

  ConfigReader::ConfigReader(EntityOwner* owner, const std::string& name, const std::string& fileName,
      const std::unordered_set<std::string>& tags)
  : ApplicationModule(owner, name, "Configuration read from file '" + fileName + "'", HierarchyModifier::none, tags),
    _fileName(fileName), _moduleTree(std::make_unique<ModuleTree>(this, name + "-ModuleTree", "")) {
    construct(fileName);
  }

  /*********************************************************************************************************************/

  void ConfigReader::construct(const std::string& fileName) {
    auto fillVariableMap = [this](const Variable& var) {
      bool processed{false};
      boost::fusion::for_each(variableMap.table, FunctorFill(this, var.type, var.name, var.value, processed));
      if(!processed) parsingError("Incorrect value '" + var.type + "' for attribute 'type' of the 'variable' tag.");
    };

    auto fillArrayMap = [this](const ChimeraTK::Array& arr) {
      // create accessor and store array value in map using functor
      bool processed{false};
      boost::fusion::for_each(arrayMap.table, ArrayFunctorFill(this, arr.type, arr.name, arr.values, processed));
      if(!processed) parsingError("Incorrect value '" + arr.type + "' for attribute 'type' of the 'variable' tag.");
    };

    auto parser = ConfigParser(fileName);
    auto v = parser.getVariableList();
    auto a = parser.getArrayList();

    for(const auto& var : *v) {
      fillVariableMap(var);
    }
    for(const auto& arr : *a) {
      fillArrayMap(arr);
    }
  }

  // workaround for std::unique_ptr static assert.
  ConfigReader::~ConfigReader() = default;
  /********************************************************************************************************************/

  void ConfigReader::parsingError(const std::string& message) {
    throw ChimeraTK::logic_error("ConfigReader: Error parsing the config file '" + _fileName + "': " + message);
  }

  /*********************************************************************************************************************/

  /** Functor to set values to the scalar accessors */
  struct FunctorSetValues {
    FunctorSetValues(ConfigReader* owner) : _owner(owner) {}

    template<typename PAIR>
    void operator()(PAIR&) const {
      // get user type and vector
      typedef typename PAIR::first_type T;
      std::unordered_map<std::string, ConfigReader::Var<T>>& theMap =
          boost::fusion::at_key<T>(_owner->variableMap.table);

      // iterate vector and set values
      for(auto& pair : theMap) {
        auto& var = pair.second;
        var._accessor = var._value;
        var._accessor.write();
      }
    }

    ConfigReader* _owner;
  };

  /*********************************************************************************************************************/

  /** Functor to set values to the array accessors */
  struct FunctorSetValuesArray {
    FunctorSetValuesArray(ConfigReader* owner) : _owner(owner) {}

    template<typename PAIR>
    void operator()(PAIR&) const {
      // get user type and vector
      typedef typename PAIR::first_type T;
      std::unordered_map<std::string, ConfigReader::Array<T>>& theMap =
          boost::fusion::at_key<T>(_owner->arrayMap.table);

      // iterate vector and set values
      for(auto& pair : theMap) {
        auto& var = pair.second;
        var._accessor = var._value;
        var._accessor.write();
      }
    }

    ConfigReader* _owner;
  };

  /*********************************************************************************************************************/

  void ConfigReader::prepare() {
    boost::fusion::for_each(variableMap.table, FunctorSetValues(this));
    boost::fusion::for_each(arrayMap.table, FunctorSetValuesArray(this));
  }

  /*********************************************************************************************************************/

  ChimeraTK::Module* ModuleTree::lookup(std::string flattened_module_name) {
    // Root node, return pointer to the ConfigReader
    if(flattened_module_name == "") {
      return dynamic_cast<Module*>(_owner);
    }
    // else look up the tree
    return get(flattened_module_name);
  }

  /*********************************************************************************************************************/

  ChimeraTK::ModuleTree* ModuleTree::get(std::string flattened_name) {
    auto root_name = root(flattened_name);
    auto remaining_branch_name = branchWithoutRoot(flattened_name);

    ModuleTree* module;

    auto r = _children.find(root_name);
    if(r == _children.end()) {
      addChildNode(root_name);
    }

    if(remaining_branch_name != "") {
      module = _children[root_name]->get(remaining_branch_name);
    }
    else {
      module = _children[root_name].get();
    }

    return module;
  }

  /*********************************************************************************************************************/

  std::unique_ptr<VariableList> ConfigParser::getVariableList() {
    if(variableList_ == nullptr) {
      std::tie(variableList_, arrayList_) = parse();
    }
    return std::move(variableList_);
  }

  /*********************************************************************************************************************/

  std::unique_ptr<ArrayList> ConfigParser::getArrayList() {
    if(arrayList_ != nullptr) {
      std::tie(variableList_, arrayList_) = parse();
    }
    return std::move(arrayList_);
  }

  /*********************************************************************************************************************/

  std::tuple<std::unique_ptr<VariableList>, std::unique_ptr<ArrayList>> ConfigParser::parse() {
    const auto root = getRootNode(*parser_);
    if(root->get_name() != "configuration") {
      error("Expected 'configuration' tag instead of: " + root->get_name());
    }

    // start with clean lists: parseModule accumulates elements into these.
    variableList_ = std::make_unique<VariableList>();
    arrayList_ = std::make_unique<ArrayList>();

    const xmlpp::Element* element = dynamic_cast<const xmlpp::Element*>(root);
    std::string parent_module_name = "";
    parseModule(element, parent_module_name);

    return std::tuple<std::unique_ptr<VariableList>, std::unique_ptr<ArrayList>>{
        std::move(variableList_), std::move(arrayList_)};
  }

  /*********************************************************************************************************************/

  void ConfigParser::parseModule(const xmlpp::Element* element, std::string parent_name) {
    auto module_name = (element->get_name() == "configuration") // root node gets special treatment
        ?
        "" :
        element->get_attribute("name")->get_value() + "/";

    parent_name += module_name;

    for(const auto& child : element->get_children()) {
      element = dynamic_cast<const xmlpp::Element*>(child);
      if(!element) {
        continue; // ignore if not an element (e.g. comment)
      }
      else if(isVariable(element)) {
        variableList_->emplace_back(prefix(parent_name, parseVariable(element)));
      }
      else if(isArray(element)) {
        arrayList_->emplace_back(prefix(parent_name, parseArray(element)));
      }
      else if(isModule(element)) {
        parseModule(element, parent_name);
      }
      else {
        error("Unknown tag: " + element->get_name());
      }
    }
  }

  /*********************************************************************************************************************/

  Variable ConfigParser::parseVariable(const xmlpp::Element* element) {
    auto name = element->get_attribute("name")->get_value();
    auto type = element->get_attribute("type")->get_value();
    auto value = element->get_attribute("value")->get_value();
    return Variable{name, type, value};
  }

  /*********************************************************************************************************************/

  Array ConfigParser::parseArray(const xmlpp::Element* element) {
    auto name = element->get_attribute("name")->get_value();
    auto type = element->get_attribute("type")->get_value();
    std::map<size_t, std::string> values = gettArrayValues(element);
    return Array{name, type, values};
  }

  /*********************************************************************************************************************/

  xmlpp::Element* ConfigParser::getRootNode(xmlpp::DomParser& parser) {
    auto root = parser.get_document()->get_root_node();
    if(root->get_name() != "configuration") {
      error("Expected 'configuration' tag instead of: " + root->get_name());
    }
    return root;
  }

  /*********************************************************************************************************************/

  void ConfigParser::error(const std::string& message) {
    throw ChimeraTK::logic_error("ConfigReader: Error parsing the config file '" + fileName_ + "': " + message);
  }

  /*********************************************************************************************************************/

  bool ConfigParser::isVariable(const xmlpp::Element* element) {
    if((element->get_name() == "variable") && element->get_attribute("value")) {
      // validate variable node
      if(!element->get_attribute("name")) {
        error("Missing attribute 'name' for the 'variable' tag.");
      }
      else if(!element->get_attribute("type")) {
        error("Missing attribute 'type' for the 'variable' tag.");
      }
      return true;
    }
    else {
      return false;
    }
  }

  /*********************************************************************************************************************/

  bool ConfigParser::isArray(const xmlpp::Element* element) {
    if((element->get_name() == "variable") && !element->get_attribute("value")) {
      // validate array node
      if(!element->get_attribute("name")) {
        error("Missing attribute 'name' for the 'variable' tag.");
      }
      else if(!element->get_attribute("type")) {
        error("Missing attribute 'type' for the 'variable' tag.");
      }
      return true;
    }
    else {
      return false;
    }
  }

  /*********************************************************************************************************************/

  bool ConfigParser::isModule(const xmlpp::Element* element) {
    if(element->get_name() == "module") {
      if(!element->get_attribute("name")) {
        error("Missing attribute 'name' for the 'module' tag.");
      }
      return true;
    }
    else {
      return false;
    }
  }

  /*********************************************************************************************************************/

  std::map<size_t, std::string> ConfigParser::gettArrayValues(const xmlpp::Element* element) {
    bool valueFound = false;
    std::map<size_t, std::string> values;

    for(const auto& valueChild : element->get_children()) {
      const xmlpp::Element* valueElement = dynamic_cast<const xmlpp::Element*>(valueChild);
      if(!valueElement) continue; // ignore comments etc.
      validateValueNode(valueElement);
      valueFound = true;

      auto index = valueElement->get_attribute("i");
      auto value = valueElement->get_attribute("v");

      // get index as number and store value as a string
      size_t intIndex;
      try {
        intIndex = std::stoi(index->get_value());
      }
      catch(std::exception& e) {
        error("Cannot parse string '" + std::string(index->get_value()) + "' as an index number: " + e.what());
      }
      values[intIndex] = value->get_value();
    }
    // make sure there is at least one value
    if(!valueFound) {
      error("Each variable must have a value, either specified as an attribute or as child tags.");
    }
    return values;
  }

  /*********************************************************************************************************************/

  void ConfigParser::validateValueNode(const xmlpp::Element* valueElement) {
    if(valueElement->get_name() != "value") {
      error("Expected 'value' tag instead of: " + valueElement->get_name());
    }
    if(!valueElement->get_attribute("i")) {
      error("Missing attribute 'index' for the 'value' tag.");
    }
    if(!valueElement->get_attribute("v")) {
      error("Missing attribute 'value' for the 'value' tag.");
    }
  }

  /*********************************************************************************************************************/

  std::unique_ptr<xmlpp::DomParser> createDomParser(const std::string& fileName) {
    try {
      return std::make_unique<xmlpp::DomParser>(fileName);
    }
    catch(xmlpp::exception& e) { /// @todo change exception!
      throw ChimeraTK::logic_error("ConfigReader: Error opening the config file '" + fileName + "': " + e.what());
    }
  }

  /*********************************************************************************************************************/

  std::string root(std::string flattened_name) {
    auto pos = flattened_name.find_first_of('/');
    pos = (pos == std::string::npos) ? flattened_name.size() : pos;
    return flattened_name.substr(0, pos);
  }

  /*********************************************************************************************************************/

  std::string branchWithoutRoot(std::string flattened_name) {
    auto pos = flattened_name.find_first_of('/');
    pos = (pos == std::string::npos) ? flattened_name.size() : pos + 1;
    return flattened_name.substr(pos, flattened_name.size());
  }

  /*********************************************************************************************************************/

  std::string branch(std::string flattened_name) {
    auto pos = flattened_name.find_last_of('/');
    pos = (pos == std::string::npos) ? 0 : pos;
    return flattened_name.substr(0, pos);
  }

  /*********************************************************************************************************************/
  std::string leaf(std::string flattened_name) {
    auto pos = flattened_name.find_last_of('/');
    return flattened_name.substr(pos + 1, flattened_name.size());
  }

  /*********************************************************************************************************************/

} // namespace ChimeraTK
