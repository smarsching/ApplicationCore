// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ModuleImpl.h"

#include "Application.h"
#include "ConfigReader.h"

namespace ChimeraTK {

  /*********************************************************************************************************************/

  ModuleImpl::ModuleImpl(EntityOwner* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : Module(owner, name, description, hierarchyModifier, tags) {}

  /*********************************************************************************************************************/

  ModuleImpl::ModuleImpl(EntityOwner* owner, const std::string& name, const std::string& description,
      bool eliminateHierarchy, const std::unordered_set<std::string>& tags)
  : Module(owner, name, description, eliminateHierarchy, tags) {}

  /*********************************************************************************************************************/

  ModuleImpl& ModuleImpl::operator=(ModuleImpl&& other) {
    if(other.virtualisedModule_isValid) virtualisedModule = other.virtualisedModule;
    virtualisedModule_isValid = other.virtualisedModule_isValid;
    Module::operator=(std::forward<ModuleImpl>(other));
    return *this;
  }

  /*********************************************************************************************************************/

  VariableNetworkNode ModuleImpl::operator()(const std::string& variableName) const {
    return virtualise()(variableName);
  }

  /*********************************************************************************************************************/

  Module& ModuleImpl::operator[](const std::string& moduleName) const {
    return virtualise()[moduleName];
  }

  /*********************************************************************************************************************/

  void ModuleImpl::connectTo(const Module& target, VariableNetworkNode trigger) const {
    virtualise().connectTo(target.virtualise(), trigger);
  }

  /*********************************************************************************************************************/

  const Module& ModuleImpl::virtualise() const {
    if(!virtualisedModule_isValid) {
      virtualisedModule = findTag(".*");
      virtualisedModule_isValid = true;
    }
    return virtualisedModule;
  }

  /*********************************************************************************************************************/

  ConfigReader& ModuleImpl::appConfig() {
    size_t nConfigReaders = 0;
    ConfigReader* instance = nullptr;
    for(auto* mod : Application::getInstance().getSubmoduleListRecursive()) {
      if(!dynamic_cast<ConfigReader*>(mod)) continue;
      ++nConfigReaders;
      instance = dynamic_cast<ConfigReader*>(mod);
    }
    if(nConfigReaders != 1) {
      std::string message = "ApplicationModule::appConfig() called but " + std::to_string(nConfigReaders) +
          " instances of ChimeraTK::ConfigReader have been found.";
      // Printing the message as well; there is a situation when running under Boost::Test where this
      // is caught by Boost and causes a weird destructor message from AppBase.cc instead with no means of
      // finding out the actual error
      std::cerr << message << std::endl;
      throw ChimeraTK::logic_error(message);
    }
    return *instance;
  }
} // namespace ChimeraTK
