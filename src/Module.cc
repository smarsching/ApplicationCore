/*
 * Module.cc
 *
 *  Created on: Jun 27, 2016
 *      Author: Martin Hierholzer
 */

#include "Application.h"
#include "Module.h"

namespace ChimeraTK {

  Module::Module(EntityOwner *owner, const std::string &name, const std::string &description,
                 bool eliminateHierarchy)
  : EntityOwner(owner, name, description, eliminateHierarchy)
  {}
  
/*********************************************************************************************************************/

  Module::~Module()
  {}
  
/*********************************************************************************************************************/

  void Module::connectTo(const Module &target, VariableNetworkNode trigger) const {
    
    // connect all direct variables of this module to their counter-parts in the right-hand-side module
    for(auto variable : getAccessorList()) {
      if(variable.getDirection() == VariableDirection::feeding) {
        variable >> target(variable.getName());
      }
      else {
        // use trigger?
        if(trigger != VariableNetworkNode() && target(variable.getName()).getMode() == UpdateMode::poll
                                            && variable.getMode() == UpdateMode::push ) {
          target(variable.getName()) [ trigger ] >> variable;
        }
        else {
          target(variable.getName()) >> variable;
        }
      }
    }
    
    // connect all sub-modules to their couter-parts in the right-hand-side module
    for(auto submodule : getSubmoduleList()) {
      submodule->connectTo(target[submodule->getName()], trigger);
    }
    
  }

/*********************************************************************************************************************/
  
  boost::shared_ptr<mtca4u::TransferElement> Module::readAny() {
    auto accessorList = getAccessorListRecursive();
    // put push-type transfer elements into a list suitable for TransferElement::readAny()
    std::list<std::reference_wrapper<mtca4u::TransferElement>> transferElementList;
    for(auto &accessor : accessorList) {
      if(!accessor.getAppAccessorNoType().isReadable()) continue;
      if(accessor.getMode() == UpdateMode::push) {
        transferElementList.emplace_back(accessor.getAppAccessorNoType());
      }
    }
    
    // wait until one of the push-type accessors receives an update
    auto ret = Application::getInstance().readAny(transferElementList);
    
    // trigger read on the poll-type accessors
    for(auto accessor : accessorList) {
      if(!accessor.getAppAccessorNoType().isReadable()) continue;
      if(accessor.getMode() == UpdateMode::poll) {
        accessor.getAppAccessorNoType().readNonBlocking();
      }
    }
    
    return ret;
  }

/*********************************************************************************************************************/
  
  void Module::readAll() {
    auto accessorList = getAccessorListRecursive();
    for(auto accessor : accessorList) {
      if(!accessor.getAppAccessorNoType().isReadable()) continue;
      accessor.getAppAccessorNoType().read();
    }
  }

/*********************************************************************************************************************/

  void Module::readAllNonBlocking() {
    auto accessorList = getAccessorListRecursive();
    for(auto accessor : accessorList) {
      if(!accessor.getAppAccessorNoType().isReadable()) continue;
      accessor.getAppAccessorNoType().readNonBlocking();
    }
  }

/*********************************************************************************************************************/

  void Module::readAllLatest() {
    auto accessorList = getAccessorListRecursive();
    for(auto accessor : accessorList) {
      if(!accessor.getAppAccessorNoType().isReadable()) continue;
      accessor.getAppAccessorNoType().readLatest();
    }
  }

/*********************************************************************************************************************/
  
  void Module::writeAll() {
    auto accessorList = getAccessorListRecursive();
    for(auto accessor : accessorList) {
      if(!accessor.getAppAccessorNoType().isWriteable()) continue;
      accessor.getAppAccessorNoType().write();
    }
  }

/*********************************************************************************************************************/
  
  VariableNetworkNode Module::operator()(const std::string& variableName) const {
    for(auto variable : getAccessorList()) {
      if(variable.getName() == variableName) return VariableNetworkNode(variable);
    }
    throw std::logic_error("Variable '"+variableName+"' is not part of the variable group '"+_name+"'.");
  }

/*********************************************************************************************************************/

  Module& Module::operator[](const std::string& moduleName) const {
    for(auto submodule : getSubmoduleList()) {
      if(submodule->getName() == moduleName) return *submodule;
    }
    throw std::logic_error("Sub-module '"+moduleName+"' is not part of the variable group '"+_name+"'.");
  }

} /* namespace ChimeraTK */
