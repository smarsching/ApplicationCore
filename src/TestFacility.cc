// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TestFacility.h"

namespace ChimeraTK {

  /********************************************************************************************************************/

  TestFacility::TestFacility(bool enableTestableMode) {
    auto pvManagers = createPVManager();
    pvManager = pvManagers.first;
    Application::getInstance().setPVManager(pvManagers.second);
    if(enableTestableMode) Application::getInstance().enableTestableMode();
    Application::getInstance().initialise();
  }

  /********************************************************************************************************************/

  void TestFacility::runApplication() const {
    Application::getInstance().testFacilityRunApplicationCalled = true;
    // send default values for all control system variables
    for(auto& pv : pvManager->getAllProcessVariables()) {
      callForTypeNoVoid(pv->getValueType(), [&pv, this](auto arg) {
        // Applies only to writeable variables. @todo FIXME It should also NOT apply for application-to-controlsystem
        // variables with a return channel, despite being writeable here!
        if(!pv->isWriteable()) return;
        // Safety check against incorrect usage
        if(pv->getVersionNumber() != VersionNumber(nullptr)) {
          throw ChimeraTK::logic_error("The variable '" + pv->getName() +
              "' has been written before TestFacility::runApplication() was called. Instead use "
              "TestFacility::setScalarDefault() resp. setArrayDefault() to set initial values.");
        }
        typedef decltype(arg) T;
        auto pv_casted = boost::dynamic_pointer_cast<NDRegisterAccessor<T>>(pv);
        auto table = boost::fusion::at_key<T>(defaults.table);
        // If default value has been stored, copy the default value to the PV.
        if(table.find(pv->getName()) != table.end()) {
          /// Since pv_casted is the undecorated PV (lacking the TestableModeAccessorDecorator), we need to copy the
          /// value also to the decorator. We still have to write through the undecorated PV, otherwise the tests are
          /// stalled. @todo It is not understood why this happens!
          /// Decorated accessors are stored in different maps for scalars are arrays...
          if(pv_casted->getNumberOfSamples() == 1) { // scalar
            auto accessor = this->getScalar<T>(pv->getName());
            accessor = table.at(pv->getName())[0];
          }
          else { // array
            auto accessor = this->getArray<T>(pv->getName());
            accessor = table.at(pv->getName());
          }
          // copy value also to undecorated PV
          pv_casted->accessChannel(0) = table.at(pv->getName());
        }
        // Write the initial value. This must be done even if no default value has been stored, since it is expected
        // by the application.
        pv_casted->write();
      });
    }
    // start the application
    Application::getInstance().run();
    // set thread name
    Application::registerThread("TestThread");
    // make sure all initial values have been propagated when in testable mode
    if(Application::getInstance().isTestableModeEnabled()) {
      // call stepApplication() only in testable mode and only if the queues are not empty
      if(Application::getInstance().testableMode_counter != 0 ||
          Application::getInstance().testableMode_deviceInitialisationCounter != 0) {
        stepApplication();
      }
    }

    // receive all initial values for the control system variables
    if(Application::getInstance().isTestableModeEnabled()) {
      for(auto& pv : pvManager->getAllProcessVariables()) {
        if(!pv->isReadable()) continue;
        callForTypeNoVoid(pv->getValueType(), [&](auto t) {
          typedef decltype(t) UserType;
          this->getArray<UserType>(pv->getName()).readNonBlocking();
        });
      }
    }
  }

  /********************************************************************************************************************/

  bool TestFacility::canStepApplication() const {
    return Application::getInstance().canStepApplication();
  }

  /********************************************************************************************************************/

  void TestFacility::stepApplication(bool waitForDeviceInitialisation) const {
    Application::getInstance().stepApplication(waitForDeviceInitialisation);
  }

  /********************************************************************************************************************/

  ChimeraTK::VoidRegisterAccessor TestFacility::getVoid(const ChimeraTK::RegisterPath& name) const {
    // check for existing accessor in cache
    if(boost::fusion::at_key<ChimeraTK::Void>(accessorMap.table).count(name) > 0) {
      return boost::fusion::at_key<ChimeraTK::Void>(accessorMap.table)[name];
    }

    // obtain accessor from ControlSystemPVManager
    auto pv = pvManager->getProcessArray<ChimeraTK::Void>(name);
    if(pv == nullptr) {
      throw ChimeraTK::logic_error("Process variable '" + name + "' does not exist.");
    }

    // obtain variable id from pvIdMap and transfer it to idMap (required by the
    // TestableModeAccessorDecorator)
    size_t varId = Application::getInstance().pvIdMap[pv->getUniqueId()];

    // decorate with TestableModeAccessorDecorator if variable is sender and
    // receiver is not poll-type, and store it in cache
    if(pv->isWriteable() && !Application::getInstance().testableMode_isPollMode[varId]) {
      auto deco = boost::make_shared<TestableModeAccessorDecorator<ChimeraTK::Void>>(pv, false, true, varId, varId);
      Application::getInstance().testableMode_names[varId] = "ControlSystem:" + name;
      boost::fusion::at_key<ChimeraTK::Void>(accessorMap.table)[name] = deco;
    }
    else {
      boost::fusion::at_key<ChimeraTK::Void>(accessorMap.table)[name] = pv;
    }

    // return the accessor as stored in the cache
    return boost::fusion::at_key<ChimeraTK::Void>(accessorMap.table)[name];
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
