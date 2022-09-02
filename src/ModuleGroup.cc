// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ModuleGroup.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags)
  : ModuleImpl(owner, name, description, tags) {
    if(!owner) {
      throw ChimeraTK::logic_error("ModuleGroup owner cannot be nullptr");
    }

    if(owner->getModel().isValid()) {
      _model = owner->getModel().add(*this);
    }
  }

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(ModuleGroup* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ModuleGroup(owner, name, description, tags) {
    applyHierarchyModifierToName(hierarchyModifier);
  }

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags)
  : ModuleGroup(dynamic_cast<ModuleGroup*>(owner), name, description, hierarchyModifier, tags) {}

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(EntityOwner* owner, const std::string& name, const std::string& description,
      bool eliminateHierarchy, const std::unordered_set<std::string>& tags)
  : ModuleGroup(dynamic_cast<ModuleGroup*>(owner), name, description,
        eliminateHierarchy ? HierarchyModifier::hideThis : HierarchyModifier::none, tags) {}

  /********************************************************************************************************************/

  ModuleGroup::ModuleGroup(const std::string& name) : ModuleImpl(nullptr, name, "") {}

  /********************************************************************************************************************/

  ModuleGroup& ModuleGroup::operator=(ModuleGroup&& other) noexcept {
    ModuleImpl::operator=(std::move(other));
    return *this;
  }

  /*********************************************************************************************************************/

  std::string ModuleGroup::getVirtualQualifiedName() const {
    return _model.getFullyQualifiedPath();
  }

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
