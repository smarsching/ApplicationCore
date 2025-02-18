// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "Application.h"
#include "InversionOfControlAccessor.h"

#include <ChimeraTK/ScalarRegisterAccessor.h>

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <string>

namespace ChimeraTK {

  /********************************************************************************************************************/

  /** Accessor for scalar variables (i.e. single values). Note for users: Use the
   * convenience classes
   *  ScalarPollInput, ScalarPushInput, ScalarOutput instead of this class
   * directly. */
  template<typename UserType>
  class ScalarAccessor : public ChimeraTK::ScalarRegisterAccessor<UserType>,
                         public InversionOfControlAccessor<ScalarAccessor<UserType>> {
   public:
    using InversionOfControlAccessor<ScalarAccessor<UserType>>::operator VariableNetworkNode;
    using InversionOfControlAccessor<ScalarAccessor<UserType>>::operator>>;
    void replace(const ChimeraTK::NDRegisterAccessorAbstractor<UserType>& newAccessor) = delete;
    using InversionOfControlAccessor<ScalarAccessor<UserType>>::replace;
    ScalarAccessor<UserType>& operator=(ScalarAccessor<UserType>& other) = delete;
    using ChimeraTK::ScalarRegisterAccessor<UserType>::operator=;

    /** Move constructor */
    ScalarAccessor(ScalarAccessor<UserType>&& other);

    /** Move assignment. */
    ScalarAccessor<UserType>& operator=(ScalarAccessor<UserType>&& other);

    bool write(ChimeraTK::VersionNumber versionNumber) = delete;
    bool writeDestructively(ChimeraTK::VersionNumber versionNumber) = delete;
    void writeIfDifferent(UserType newValue, VersionNumber versionNumber) = delete;

    bool write();

    bool writeDestructively();

    void writeIfDifferent(UserType newValue);

   protected:
    friend class InversionOfControlAccessor<ScalarAccessor<UserType>>;

    ScalarAccessor(Module* owner, const std::string& name, VariableDirection direction, std::string unit,
        UpdateMode mode, const std::string& description, const std::unordered_set<std::string>& tags = {});

    /** Default constructor creates a dysfunctional accessor (to be assigned with
     * a real accessor later) */
    ScalarAccessor() = default;
  };

  /********************************************************************************************************************/

  /** Convenience class for input scalar accessors with UpdateMode::push */
  template<typename UserType>
  struct ScalarPushInput : public ScalarAccessor<UserType> {
    ScalarPushInput(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {});
    ScalarPushInput() : ScalarAccessor<UserType>() {}
    using ScalarAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for input scalar accessors with UpdateMode::poll */
  template<typename UserType>
  struct ScalarPollInput : public ScalarAccessor<UserType> {
    ScalarPollInput(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {});
    ScalarPollInput() : ScalarAccessor<UserType>() {}
    void read() { this->readLatest(); }
    using ScalarAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for output scalar accessors (always UpdateMode::push) */
  template<typename UserType>
  struct ScalarOutput : public ScalarAccessor<UserType> {
    ScalarOutput(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {});
    ScalarOutput() : ScalarAccessor<UserType>() {}
    using ScalarAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for input scalar accessors with return channel ("write back") and UpdateMode::push. */
  template<typename UserType>
  struct ScalarPushInputWB : public ScalarAccessor<UserType> {
    ScalarPushInputWB(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {});
    ScalarPushInputWB() : ScalarAccessor<UserType>() {}
    using ScalarAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/

  /** Convenience class for output scalar accessors with return channel ("read back") (always UpdateMode::push) */
  template<typename UserType>
  struct ScalarOutputPushRB : public ScalarAccessor<UserType> {
    ScalarOutputPushRB(Module* owner, const std::string& name, std::string unit, const std::string& description,
        const std::unordered_set<std::string>& tags = {});
    ScalarOutputPushRB() : ScalarAccessor<UserType>() {}
    using ScalarAccessor<UserType>::operator=;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /* Implementations below this point                                                                                 */
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ScalarAccessor<UserType>::ScalarAccessor(ScalarAccessor<UserType>&& other) {
    InversionOfControlAccessor<ScalarAccessor<UserType>>::replace(std::move(other));
  }

  /********************************************************************************************************************/

  template<typename UserType>
  ScalarAccessor<UserType>& ScalarAccessor<UserType>::operator=(ScalarAccessor<UserType>&& other) {
    // Having a move-assignment operator is required to use the move-assignment
    // operator of a module containing an accessor.
    InversionOfControlAccessor<ScalarAccessor<UserType>>::replace(std::move(other));
    return *this;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool ScalarAccessor<UserType>::write() {
    auto versionNumber = this->getOwner()->getCurrentVersionNumber();
    bool dataLoss = ChimeraTK::ScalarRegisterAccessor<UserType>::write(versionNumber);
    if(dataLoss) Application::incrementDataLossCounter(this->node.getQualifiedName());
    return dataLoss;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  bool ScalarAccessor<UserType>::writeDestructively() {
    auto versionNumber = this->getOwner()->getCurrentVersionNumber();
    bool dataLoss = ChimeraTK::ScalarRegisterAccessor<UserType>::writeDestructively(versionNumber);
    if(dataLoss) Application::incrementDataLossCounter(this->node.getQualifiedName());
    return dataLoss;
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void ScalarAccessor<UserType>::writeIfDifferent(UserType newValue) {
    auto versionNumber = this->getOwner()->getCurrentVersionNumber();
    ChimeraTK::ScalarRegisterAccessor<UserType>::writeIfDifferent(newValue, versionNumber);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  ScalarAccessor<UserType>::ScalarAccessor(Module* owner, const std::string& name, VariableDirection direction,
      std::string unit, UpdateMode mode, const std::string& description, const std::unordered_set<std::string>& tags)
  : InversionOfControlAccessor<ScalarAccessor<UserType>>(
        owner, name, direction, unit, 1, mode, description, &typeid(UserType), tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ScalarPushInput<UserType>::ScalarPushInput(Module* owner, const std::string& name, std::string unit,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ScalarAccessor<UserType>(
        owner, name, {VariableDirection::consuming, false}, unit, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ScalarPollInput<UserType>::ScalarPollInput(Module* owner, const std::string& name, std::string unit,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ScalarAccessor<UserType>(
        owner, name, {VariableDirection::consuming, false}, unit, UpdateMode::poll, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ScalarOutput<UserType>::ScalarOutput(Module* owner, const std::string& name, std::string unit,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ScalarAccessor<UserType>(
        owner, name, {VariableDirection::feeding, false}, unit, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ScalarPushInputWB<UserType>::ScalarPushInputWB(Module* owner, const std::string& name, std::string unit,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ScalarAccessor<UserType>(
        owner, name, {VariableDirection::consuming, true}, unit, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  ScalarOutputPushRB<UserType>::ScalarOutputPushRB(Module* owner, const std::string& name, std::string unit,
      const std::string& description, const std::unordered_set<std::string>& tags)
  : ScalarAccessor<UserType>(
        owner, name, {VariableDirection::feeding, true}, unit, UpdateMode::push, description, tags) {}

  /********************************************************************************************************************/

} /* namespace ChimeraTK */
