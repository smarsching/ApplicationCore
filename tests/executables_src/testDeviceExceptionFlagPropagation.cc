// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE testDeviceExceptionFlagPropagation

#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "Application.h"
#include "ApplicationModule.h"
#include "check_timeout.h"
#include "ControlSystemModule.h"
#include "DeviceModule.h"
#include "PeriodicTrigger.h"
#include "TestFacility.h"
#include "VariableGroup.h"

#include <ChimeraTK/DummyRegisterAccessor.h>
#include <ChimeraTK/ExceptionDummyBackend.h>

namespace ctk = ChimeraTK;

constexpr char ExceptionDummyCDD1[] = "(ExceptionDummy:1?map=test3.map)";

struct TestApplication : ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() { shutdown(); }

  void defineConnections() {}

  struct Name : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct Name2 : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarOutput<uint64_t> tick{this, "tick", "", ""};
    } name{(this), "name", ""}; // extra parentheses are for doxygen...

    void prepare() override { name.tick.write(); /* send initial value */ }
    void mainLoop() override {}
  } name{this, "name", ""};

  struct Module : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    mutable int readMode{0};

    struct Vars : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<uint64_t> tick{this, "tick", "", ""};
      ctk::ScalarPollInput<int> read{this, "readBack", "", ""};
      ctk::ScalarOutput<int> set{this, "actuator", "", ""};
    } vars{this, "vars", "", ctk::HierarchyModifier::hideThis};

    std::atomic<ctk::DataValidity> readDataValidity;

    void prepare() override {
      readDataValidity = vars.read.dataValidity();
      // The receiving end of all accessor implementations should be constructed with faulty (Initial value propagation
      // spec, D.1)
      BOOST_CHECK(readDataValidity == ctk::DataValidity::faulty);
      vars.set.write(); /* send intial value */
    }

    void mainLoop() override {
      readDataValidity = vars.read.dataValidity();
      while(true) {
        vars.tick.read();
        readDataValidity = vars.read.dataValidity();
        switch(readMode) {
          case 0:
            vars.read.readNonBlocking();
            break;
          case 1:
            vars.read.readLatest();
            break;
          case 2:
            vars.read.read();
            break;
          case 3:
            vars.set.write();
            break;
          case 4:
            vars.set.writeDestructively();
            break;
          default:
            break;
        }
      }
    }

  } module{this, "module", ""};

  ctk::PeriodicTrigger trigger{this, "trigger", ""};

  ctk::DeviceModule dev{this, ExceptionDummyCDD1};
  ctk::ControlSystemModule cs;
};

BOOST_AUTO_TEST_CASE(testDirectConnectOpen) {
  std::cout << "testDirectConnectOpen" << std::endl;
  for(int readMode = 0; readMode < 2; ++readMode) {
    TestApplication app;

    boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
        ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1));

    app.dev("/MyModule/readBack", typeid(int), 1) >> app.module.vars.read;
    app.module.vars.set >> app.dev("/MyModule/actuator", typeid(int), 1);
    app.name.name.tick >> app.module.vars.tick;

    ctk::TestFacility test(false);

    // Throw on device open and check if DataValidity::faulty gets propagated
    dummyBackend1->throwExceptionOpen = true;
    // set the read mode
    app.module.readMode = readMode;
    std::cout << "Read mode is: " << app.module.readMode << ". Run application.\n";
    app.run();
    CHECK_EQUAL_TIMEOUT(test.readScalar<int>("Devices/" + std::string(ExceptionDummyCDD1) + "/status"), 1, 10000);

    // Trigger and check
    app.name.name.tick.write();
    usleep(10000);
    BOOST_CHECK(app.module.readDataValidity == ctk::DataValidity::faulty);

    // recover from error state
    dummyBackend1->throwExceptionOpen = false;
    CHECK_TIMEOUT(app.module.readDataValidity == ctk::DataValidity::ok, 10000);
  }
}

BOOST_AUTO_TEST_CASE(testDirectConnectRead) {
  std::cout << "testDirectConnectRead" << std::endl;
  TestApplication app;
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1));

  app.dev("/MyModule/readBack", typeid(int), 1) >> app.module.vars.read;
  app.module.vars.set >> app.dev("/MyModule/actuator", typeid(int), 1);
  app.trigger.tick >> app.module.vars.tick;

  ctk::TestFacility test(true);
  test.runApplication();

  // Advance through all read methods
  while(app.module.readMode < 3) {
    // Check
    app.trigger.sendTrigger();
    test.stepApplication();
    BOOST_CHECK(app.module.vars.read.dataValidity() == ctk::DataValidity::ok);

    // Check
    std::cout << "Checking read mode " << app.module.readMode << "\n";
    dummyBackend1->throwExceptionRead = true;
    app.trigger.sendTrigger();
    test.stepApplication(false);
    BOOST_CHECK(app.module.vars.read.dataValidity() == ctk::DataValidity::faulty);

    // Reset throwing and let the device recover
    dummyBackend1->throwExceptionRead = false;
    test.stepApplication(true);

    // advance to the next read
    app.module.readMode++;
  }
}

BOOST_AUTO_TEST_CASE(testDirectConnectWrite) {
  std::cout << "testDirectConnectWrite" << std::endl;
  TestApplication app;
  boost::shared_ptr<ctk::ExceptionDummy> dummyBackend1 = boost::dynamic_pointer_cast<ctk::ExceptionDummy>(
      ChimeraTK::BackendFactory::getInstance().createBackend(ExceptionDummyCDD1));

  app.dev("/MyModule/readBack", typeid(int), 1) >> app.module.vars.read;
  app.module.vars.set >> app.dev("/MyModule/actuator", typeid(int), 1);
  app.module.readMode = 3;
  app.trigger.tick >> app.module.vars.tick;

  ctk::TestFacility test(true);
  test.runApplication();

  // Advance through all non-blocking read methods
  while(app.module.readMode < 5) {
    // Check
    app.trigger.sendTrigger();
    test.stepApplication();
    BOOST_CHECK(app.module.vars.set.dataValidity() == ctk::DataValidity::ok);

    // Check
    dummyBackend1->throwExceptionWrite = true;
    app.trigger.sendTrigger();
    test.stepApplication(false);
    // write operations failing does not invalidate data
    BOOST_CHECK(app.module.vars.set.dataValidity() == ctk::DataValidity::ok);

    // advance to the next read
    dummyBackend1->throwExceptionWrite = false;
    app.module.readMode++;
  }
}
