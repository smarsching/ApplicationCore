/*
 * EntityOwner.h
 *
 *  Created on: Nov 15, 2016
 *      Author: Martin Hierholzer
 */

#ifndef CHIMERATK_ENTITY_OWNER_H
#define CHIMERATK_ENTITY_OWNER_H

#include <string>
#include <list>

#include "VariableNetworkNode.h"

namespace ChimeraTK {
  
  class AccessorBase;
  class Module;
  class VirtualModule;

  /** Base class for owners of other EntityOwners (e.g. Modules) and Accessors.
    * @todo Rename this class to "Owner" and make it more generic. It should basically just implement the
    * "Composite Pattern". The classes AccessorBase, Module and Owner should have a common base class called
    * "Component".
    */
  class EntityOwner {

    public:

      /** Constructor: register the EntityOwner with its owner */
      EntityOwner(EntityOwner *owner, const std::string &name, const std::string &description,
                  bool eliminateHierarchy=false, const std::unordered_set<std::string> &tags={});
      
      /** Virtual destructor to make the type polymorphic */
      virtual ~EntityOwner();
      
      /** Move constructor */
      EntityOwner(EntityOwner &&other)
      : _name(std::move(other._name)),
        _description(std::move(other._description)),
        _owner(other._owner),
        accessorList(std::move(other.accessorList)),
        moduleList(std::move(other.moduleList)),
        _eliminateHierarchy(other._eliminateHierarchy),
        _tags(std::move(other._tags))
      {}

      /** Move operation with the assignment operator */
      EntityOwner& operator=(EntityOwner &&other) {
        _name = std::move(other._name);
        _description = std::move(other._description);
        _owner = std::move(other._owner);
        accessorList = std::move(other.accessorList);
        moduleList = std::move(other.moduleList);
        _eliminateHierarchy = other._eliminateHierarchy;
        _tags = std::move(other._tags);
        return *this;
      }
      
      /** Delete other assignment operators */
      EntityOwner& operator=(EntityOwner &other) = delete;
      EntityOwner& operator=(const EntityOwner &other) = delete;

      /** Get the name of the module instance */
      const std::string& getName() const { return _name; }

      /** Get the fully qualified name of the module instance, i.e. the name containing all module names further up in
       *  the hierarchy. */
      std::string getQualifiedName() const {
        return ( _owner != nullptr ? _owner->getQualifiedName() : "" ) + "/" + _name;
      }
      
      /** Get the decription of the module instance */
      const std::string& getDescription() const { return _description; }
      
      /** Obtain the list of accessors/variables directly associated with this instance */
      std::list<VariableNetworkNode>& getAccessorList() { return accessorList; }
      const std::list<VariableNetworkNode>& getAccessorList() const { return accessorList; }
      
      /** Obtain the list of submodules associated with this instance */
      const std::list<Module*>& getSubmoduleList() const { return moduleList; }
      
      /** Obtain the list of accessors/variables associated with this instance and any submodules */
      std::list<VariableNetworkNode> getAccessorListRecursive();
      
      /** Obtain the list of submodules associated with this instance and any submodules */
      std::list<Module*> getSubmoduleListRecursive();
      
      /** Return a VirtualModule containing the part of the tree structure matching the given tag. The resulting
       *  VirtualModule might have virtual sub-modules, if this EntityOwner contains sub-EntityOwners with
       *  entities matchting the tag. */
      VirtualModule findTag(const std::string &tag, bool eliminateAllHierarchies=false) const;
      
      /** Add the part of the tree structure matching the given tag to a VirtualModule. Users normally will use
       *  findTag() instead. */
      void findTagAndAppendToModule(VirtualModule &module, const std::string &tag, bool eliminateAllHierarchies=false,
                                    bool eliminateFirstHierarchy=false) const;

      /** Called inside the constructor of Accessor: adds the accessor to the list */
      void registerAccessor(VariableNetworkNode accessor) {
        for(auto &tag : _tags) accessor.addTag(tag);
        accessorList.push_back(accessor);
      }

      /** Called inside the destructor of Accessor: removes the accessor from the list */
      void unregisterAccessor(VariableNetworkNode accessor) {
        accessorList.remove(accessor);
      }
      
      /** Register another module as a sub-mdoule. Will be called automatically by all modules in their constructors. */
      void registerModule(Module* module);
      
      /** Unregister another module as a sub-mdoule. Will be called automatically by all modules in their destructors. */
      void unregisterModule(Module* module);

      /** Add a tag to all Application-type nodes inside this group. It will recurse into any subgroups. See
       *  VariableNetworkNode::addTag() for additional information about tags. */
      void addTag(const std::string &tag);
      
      /** Eliminate the level of hierarchy represented by this EntityOwner. This is e.g. used when building the
       *  hierarchy of VirtualModules in findTag(). Eliminating one level of hierarchy will make all childs of that
       *  hierarchy level to appear as if there were direct childs of the next higher hierarchy level. If e.g. there is
       *  a variable on the third level "A.B.C" and one selects to eliminate the second level of hierarchy (e.g. calls
       *  B.eliminateHierarchy()), the structure would look like "A.C". This of course only affects the "dynamic" data
       *  model, while the static C++ model is fixed at compile time.
       *  @todo Also use in VariableGroup::operator() and VariableGroup::operator[] ??? */
      void setEliminateHierarchy() { _eliminateHierarchy = true; }
      
      /** Returns the flag whether this level of hierarchy should be eliminated */
      bool getEliminateHierarchy() const { return _eliminateHierarchy; }
      
      /** Create a VirtualModule which contains all variables of this EntityOwner in a flat hierarchy. It will recurse
       *  through all sub-modules and add all found variables directly to the VirtualModule. */
      VirtualModule flatten();
      
      /** Print the full hierarchy to stdout. */
      void dump(const std::string &prefix="") const;
      
      /** Create Graphviz dot graph write to file */
      void dumpGraph(const std::string &fileName="graph.dot") const;
      
      enum class ModuleType {
        ApplicationModule, ModuleGroup, VariableGroup, ControlSystem, Device
      };
      
      /** Return the module type of this module, or in case of a VirtualModule the module type this VirtualModule was
       *  derived from. */
      virtual ModuleType getModuleType() const = 0;

  protected:
      
      /** Create Graphviz dot graph write to stream, excluding the surrounding digraph command */
      void dumpGraphInternal(std::ostream &stream) const;
      
      /** Clean a fully qualified entity name so it can be used as a dot node name (i.e. strip slashes etc.) */
      std::string cleanDotNode(std::string fullName) const;
    
      /** The name of this instance */
      std::string _name;
    
      /** The description of this instance */
      std::string _description;
      
      /** Owner of this instance */
      EntityOwner *_owner{nullptr};
      
      /** List of accessors owned by this instance */
      std::list<VariableNetworkNode> accessorList;

      /** List of modules owned by this instance */
      std::list<Module*> moduleList;
      
      /** Flag whether this level of hierarchy should be eliminated or not */
      bool _eliminateHierarchy{false};
      
      /** List of tags to be added to all accessors and modules inside this module */
      std::unordered_set<std::string> _tags;
      
  };

} /* namespace ChimeraTK */

#endif /* CHIMERATK_ENTITY_OWNER_H */

