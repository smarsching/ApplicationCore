// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "HierarchyModifyingGroup.h"
#include "ScalarAccessor.h"
#include "StatusAccessor.h"
#include "VariableGroup.h"

#include <ChimeraTK/ControlSystemAdapter/StatusWithMessageReader.h>
#include <ChimeraTK/DataConsistencyGroup.h>
#include <ChimeraTK/ForwardDeclarations.h>
#include <ChimeraTK/RegisterPath.h>

namespace ChimeraTK {

  /**
   *  A VariableGroup for error status and message reporting.
   *  Convenience methods ensure that status and message are updated consistenly.
   */
  struct StatusWithMessage : HierarchyModifyingGroup {
    StatusWithMessage(EntityOwner* owner, std::string qualifiedStatusVariableName, const std::string& description = "",
        const std::unordered_set<std::string>& tags = {})
    : HierarchyModifyingGroup(
          owner, HierarchyModifyingGroup::getPathName(qualifiedStatusVariableName), description, tags),
      _status(this, HierarchyModifyingGroup::getUnqualifiedName(qualifiedStatusVariableName), description),
      _message(this, HierarchyModifyingGroup::getUnqualifiedName(qualifiedStatusVariableName) + "_message", "",
          "status message") {}
    StatusWithMessage() {}

    /// to be use only for status != OK
    void write(StatusOutput::Status status, std::string message) {
      assert(status != StatusOutput::Status::OK);
      _status = status;
      _message = message;
      writeAll();
    }
    void writeOk() {
      _status = StatusOutput::Status::OK;
      _message = "";
      writeAll();
    }
    StatusOutput _status;
    ScalarOutput<std::string> _message;
  };

  /**
   * This is for consistent readout of StatusWithMessage - ApplicationCore version.
   *  It can be instantiated with or without message string.
   *  If instantiated without message, the message is generated automatically from the status.
   */
  struct StatusWithMessageInput : StatusWithMessageReaderBase<StatusWithMessageInput>, public VariableGroup {
    StatusPushInput _status;
    ScalarPushInput<std::string> _message; // left uninitialized, if no message source provided

    /// Construct StatusWithMessageInput which reads only status, not message
    StatusWithMessageInput(Module* owner, std::string name, const std::string& description,
        HierarchyModifier hierarchyModifier = HierarchyModifier::none, const std::unordered_set<std::string>& tags = {})
    : VariableGroup(owner, name, "", hierarchyModifier, tags), _status(this, name, description) {
      hasMessageSource = false;
      _statusNameLong = description;
    }
    /// read associated status message from given (fully qualified) msgInputName.
    ///  If not given, it is selected automatically by the naming convention
    void setMessageSource(std::string msgInputName = "") {
      // at the time this function is called, TransferElement impl is not yet set, so don't look there for name
      if(msgInputName == "") msgInputName = ((VariableNetworkNode)_status).getName() + "_message";
      // late initialization of _message
      _message = ScalarPushInput<std::string>(this, msgInputName, "", "");
      hasMessageSource = true;
    }
  };

} /* namespace ChimeraTK */
