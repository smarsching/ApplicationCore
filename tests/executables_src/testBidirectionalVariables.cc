/*
 * testBidirectionalVariables.cc
 *
 *  Created on: Jan 21, 2019
 *      Author: Martin Hierholzer
 */

#include <future>

#define BOOST_TEST_MODULE testBidirectionalVariables

#include <boost/mpl/list.hpp>

#include <ChimeraTK/BackendFactory.h>

#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ControlSystemModule.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"

#include "check_timeout.h"

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

/*********************************************************************************************************************/

/* Module which converts the input data from inches to centimeters - and the
 * other way round for the return channel. In case of the return channel, the
 * data is rounded downwards to integer inches and sent again forward. */
struct ModuleA : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInputWB<int> var1{this, "var1", "inches", "A length, for some reason rounded to integer"};
  ctk::ScalarOutputPushRB<double> var2{this, "var2", "centimeters", "Same length converted to centimeters"};

  void prepare() override {
    incrementDataFaultCounter(); // foce all outputs  to invalid
    writeAll();                  // write initial values
    decrementDataFaultCounter(); // validity according to input validity
  }

  void mainLoop() override {
    auto group = readAnyGroup();
    while(true) {
      auto var = group.readAny();
      if(var == var2.getId()) {
        var1 = std::floor(var2 / 2.54);
        var1.write();
      }
      var2 = var1 * 2.54;
      var2.write();
    }
  }
};

/*********************************************************************************************************************/

/* Module which limits a value to stay below a maximum value. */
struct ModuleB : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInputWB<double> var2{this, "var2", "centimeters", "Some length, confined to a configuratble range"};
  ctk::ScalarPushInput<double> max{this, "max", "centimeters", "Maximum length"};
  ctk::ScalarOutput<double> var3{this, "var3", "centimeters", "The limited length"};

  void prepare() override {
    incrementDataFaultCounter(); // foce all outputs  to invalid
    writeAll();                  // write initial values
    decrementDataFaultCounter(); // validity according to input validity
  }

  void mainLoop() override {
    auto group = readAnyGroup();
    while(true) {
      auto var = group.readAny();
      bool write = var == var2.getId();
      if(var2 > max) {
        var2 = static_cast<double>(max);
        var2.write();
        write = true;
      }
      if(write) { // write only if var2 was received or the value was changed
                  // due to a reduced limit
        var3 = static_cast<double>(var2);
        var3.write();
      }
    }
  }
};

/*********************************************************************************************************************/

struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() { shutdown(); }

  using Application::makeConnections; // we call makeConnections() manually in
                                      // the tests to catch exceptions etc.
  void defineConnections() {}         // the setup is done in the tests
  ctk::ControlSystemModule cs;
  ModuleA a;
  ModuleB b;
};

/*********************************************************************************************************************/

struct ModuleC : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInputWB<int> var1{this, "var1", "", ""};

  void mainLoop() override {
    auto group = readAnyGroup();

    var1 = 42;
    var1.write();

    while(true) {
      auto var = group.readAny();
      if(var == var1.getId()) {
        var1 = var1 + 1;
        var1.write();
      }
    }
  }
};

/*********************************************************************************************************************/

struct InitTestApplication : public ctk::Application {
  InitTestApplication() : Application("testSuite") {}
  ~InitTestApplication() { shutdown(); }

  void defineConnections() { findTag(".*").connectTo(cs); }

  ctk::ControlSystemModule cs;
  ModuleC c{this, "ModuleC", ""};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDirectAppToCSConnections) {
  std::cout << "*** testDirectAppToCSConnections" << std::endl;

  TestApplication app;
  app.b = {&app, "b", ""};
  app.b.connectTo(app.cs);

  ctk::TestFacility test;
  test.runApplication();
  auto var2 = test.getScalar<double>("var2");
  auto var3 = test.getScalar<double>("var3");
  auto max = test.getScalar<double>("max");

  // set maximum in B
  max = 49.5;
  max.write();
  test.stepApplication();

  // inject value which does not get limited
  var2 = 49;
  var2.write();
  test.stepApplication();
  var3.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var3), 49, 0.001);
  BOOST_CHECK(var2.readNonBlocking() == false);
  BOOST_CHECK(var3.readNonBlocking() == false);

  // inject value which gets limited
  var2 = 50;
  var2.write();
  test.stepApplication();
  var2.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var2), 49.5, 0.001);
  var3.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var3), 49.5, 0.001);
  BOOST_CHECK(var2.readNonBlocking() == false);
  BOOST_CHECK(var3.readNonBlocking() == false);

  // change the limit so the current value gets changed
  max = 48.5;
  max.write();
  test.stepApplication();
  var2.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var2), 48.5, 0.001);
  var3.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var3), 48.5, 0.001);
  BOOST_CHECK(var2.readNonBlocking() == false);
  BOOST_CHECK(var3.readNonBlocking() == false);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testRealisticExample) {
  std::cout << "*** testRealisticExample" << std::endl;

  TestApplication app;
  app.a = {&app, "a", ""};
  app.b = {&app, "b", ""};

  // the connections will result in a FeedingFanOut for var2, as it is connected
  // to the control system as well
  app.a.connectTo(app.cs);
  app.b.connectTo(app.cs);
  app.a.var1 >> app.cs("var1_copied"); // add a ThreadedFanOut with return channel as well...
  ctk::TestFacility test;
  auto var1 = test.getScalar<int>("var1");
  auto var1_copied = test.getScalar<int>("var1_copied");
  auto var2 = test.getScalar<double>("var2");
  auto var3 = test.getScalar<double>("var3");
  auto max = test.getScalar<double>("max");
  test.runApplication();

  // set maximum in B, so that var1=49 is still below maximum but var2=50 is
  // already above and rounding in ModuleB will change the value again
  max = 49.5 * 2.54;
  max.write();
  test.stepApplication();

  // inject value which does not get limited
  var1 = 49;
  var1.write();
  test.stepApplication();
  var1_copied.read();
  var2.read();
  var3.read();
  BOOST_CHECK_EQUAL(static_cast<int>(var1_copied), 49);
  BOOST_CHECK_CLOSE(static_cast<double>(var2), 49 * 2.54, 0.001);
  BOOST_CHECK_CLOSE(static_cast<double>(var3), 49 * 2.54, 0.001);
  BOOST_CHECK(var1.readNonBlocking() == false); // nothing was sent through the return channel
  BOOST_CHECK(var1_copied.readLatest() == false);
  BOOST_CHECK(var2.readNonBlocking() == false);
  BOOST_CHECK(var3.readNonBlocking() == false);

  // inject value which gets limited
  var1 = 50;
  var1.write();
  test.stepApplication();
  var1.read();
  BOOST_CHECK_EQUAL(static_cast<int>(var1), 49);
  var1_copied.read();
  BOOST_CHECK_EQUAL(static_cast<int>(var1_copied), 50);
  var1_copied.read();
  BOOST_CHECK_EQUAL(static_cast<int>(var1_copied), 49);
  var2.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var2), 50 * 2.54, 0.001);
  var2.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var2), 49.5 * 2.54, 0.001);
  var2.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var2), 49 * 2.54, 0.001);
  var3.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var3), 49.5 * 2.54, 0.001);
  var3.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var3), 49 * 2.54, 0.001);
  BOOST_CHECK(var1.readNonBlocking() == false);
  BOOST_CHECK(var1_copied.readLatest() == false);
  BOOST_CHECK(var2.readNonBlocking() == false);
  BOOST_CHECK(var3.readNonBlocking() == false);

  // change the limit so the current value gets changed
  max = 48.5 * 2.54;
  max.write();
  test.stepApplication();
  var1.read();
  BOOST_CHECK_EQUAL(static_cast<int>(var1), 48);
  var1_copied.read();
  BOOST_CHECK_EQUAL(static_cast<int>(var1_copied), 48);
  var2.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var2), 48.5 * 2.54, 0.001);
  var2.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var2), 48 * 2.54, 0.001);
  var3.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var3), 48.5 * 2.54, 0.001);
  var3.read();
  BOOST_CHECK_CLOSE(static_cast<double>(var3), 48 * 2.54, 0.001);
  BOOST_CHECK(var1.readNonBlocking() == false);
  BOOST_CHECK(var1_copied.readLatest() == false);
  BOOST_CHECK(var2.readNonBlocking() == false);
  BOOST_CHECK(var3.readNonBlocking() == false);

  // Run the following tests a couple of times, as they are testing for the
  // absence of race conditions. This makes it more likely to find failures in a
  // single run of the test
  for(size_t i = 0; i < 10; ++i) {
    // feed in some default values (so the tests can be executed multiple times
    // in a row)
    max = 48.5 * 2.54;
    max.write();
    test.stepApplication();
    var1 = 50;
    var1.write();
    test.stepApplication();
    var1.readLatest(); // emtpy the queues
    var1_copied.readLatest();
    var2.readLatest();
    var3.readLatest();
    BOOST_CHECK_EQUAL(static_cast<int>(var1), 48);
    BOOST_CHECK_EQUAL(static_cast<int>(var1_copied), 48);
    BOOST_CHECK_CLOSE(static_cast<double>(var2), 48 * 2.54, 0.001);
    BOOST_CHECK_CLOSE(static_cast<double>(var3), 48 * 2.54, 0.001);
    BOOST_CHECK(var1.readNonBlocking() == false);
    BOOST_CHECK(var1_copied.readLatest() == false);
    BOOST_CHECK(var2.readNonBlocking() == false);
    BOOST_CHECK(var3.readNonBlocking() == false);

    // concurrent change of value and limit. Note: The final result must be
    // deterministic, but which values are seen in between is subject to race
    // conditions between the two concurrent updates. Thus we are using
    // readLatest() in some cases here.
    var1 = 30;
    max = 25.5 * 2.54;
    var1.write();
    max.write();
    test.stepApplication();
    var1.read();
    BOOST_CHECK_EQUAL(static_cast<int>(var1), 25);
    var1_copied.read();
    BOOST_CHECK_EQUAL(static_cast<int>(var1_copied), 30);
    BOOST_CHECK(var1_copied.readLatest() == true);
    BOOST_CHECK_EQUAL(static_cast<int>(var1_copied), 25);
    BOOST_CHECK(var2.readLatest() == true);
    BOOST_CHECK_CLOSE(static_cast<double>(var2), 25 * 2.54, 0.001);
    BOOST_CHECK(var3.readLatest() == true);
    BOOST_CHECK_CLOSE(static_cast<double>(var3), 25 * 2.54, 0.001);
    BOOST_CHECK(var1.readNonBlocking() == false);
    BOOST_CHECK(var1_copied.readLatest() == false);
    BOOST_CHECK(var2.readNonBlocking() == false);
    BOOST_CHECK(var3.readNonBlocking() == false);

    // concurrent change of value and limit - other order than before
    var1 = 15;
    max = 20.5 * 2.54;
    max.write();
    var1.write();
    test.stepApplication();
    var1_copied.read();
    BOOST_CHECK_EQUAL(static_cast<int>(var1_copied), 15);
    BOOST_CHECK(var2.readLatest() == true);
    BOOST_CHECK_CLOSE(static_cast<double>(var2), 15 * 2.54, 0.001);
    BOOST_CHECK(var3.readLatest() == true);
    BOOST_CHECK_CLOSE(static_cast<double>(var3), 15 * 2.54, 0.001);
    BOOST_CHECK(var1.readNonBlocking() == false);
    BOOST_CHECK(var1_copied.readLatest() == false);
    BOOST_CHECK(var2.readNonBlocking() == false);
    BOOST_CHECK(var3.readNonBlocking() == false);
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testStartup) {
  std::cout << "*** testStartup" << std::endl;

  InitTestApplication testApp;
  ChimeraTK::TestFacility testFacility;

  //  testApp.dump();
  //  testApp.dumpConnections();

  testFacility.setScalarDefault<int>("/ModuleC/var1", 22);

  testFacility.runApplication();

  // The default value should be overwritten when ModuleC
  // enters its mainLoop
  BOOST_CHECK_EQUAL(testFacility.readScalar<int>("ModuleC/var1"), 42);
}

/*********************************************************************************************************************/

struct TestApplication2 : ctk::Application {
  TestApplication2() : Application("testSuite") {}
  ~TestApplication2() override { shutdown(); }

  void defineConnections() override { lower.connectTo(upper); }

  template<typename ACCESSOR>
  struct Module : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    ACCESSOR var{this, "var", "", ""};

    bool sendInitialValue{ctk::VariableNetworkNode(var).getDirection().dir == ctk::VariableDirection::feeding};
    void prepare() override {
      if(sendInitialValue) {
        var.write();
      }
    }

    boost::latch mainLoopStarted{1};
    void mainLoop() override { mainLoopStarted.count_down(); }
  };
  Module<ctk::ScalarPushInputWB<int>> lower{this, "Lower", ""};
  Module<ctk::ScalarOutputPushRB<int>> upper{this, "Upper", ""};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testReadWriteAll) {
  std::cout << "*** testReadWriteAll" << std::endl;

  TestApplication2 app;
  ChimeraTK::TestFacility test;

  test.runApplication();

  // forward channel writeAll/readAll
  app.upper.var = 42;
  app.upper.writeAll();
  app.lower.readAll();
  BOOST_CHECK_EQUAL(app.lower.var, 42);

  // return channel writeAll
  app.lower.var = 43;
  app.lower.writeAll();
  BOOST_CHECK(app.upper.var.readNonBlocking() == false);

  // return channel readAll
  app.lower.var.write();
  app.upper.readAll();
  BOOST_CHECK_NE(app.upper.var, 43);
  BOOST_CHECK(app.upper.var.readNonBlocking() == true);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDataValidityReturn) {
  std::cout << "*** testDataValidityReturn" << std::endl;

  // forward channel
  {
    TestApplication2 app;
    ChimeraTK::TestFacility test;

    test.runApplication();
    assert(app.lower.getDataValidity() == ctk::DataValidity::ok);

    app.upper.incrementDataFaultCounter();
    app.upper.var = 666;
    app.upper.var.write();
    app.upper.decrementDataFaultCounter();
    app.lower.var.read();
    BOOST_CHECK(app.lower.var.dataValidity() == ctk::DataValidity::faulty);
    BOOST_CHECK(app.lower.getDataValidity() == ctk::DataValidity::faulty);
  }

  // return channel
  {
    TestApplication2 app;
    ChimeraTK::TestFacility test;

    test.runApplication();
    assert(app.upper.getDataValidity() == ctk::DataValidity::ok);
    app.lower.incrementDataFaultCounter();
    app.lower.var = 120;
    app.lower.var.write();
    app.upper.var.read();
    BOOST_CHECK(app.upper.var.dataValidity() == ctk::DataValidity::faulty);

    //=====================================================================
    // TODO This check would fail - intended behaviour inclear!!!
    //
    // See redmine issue #8607
    //
    // BOOST_CHECK(app.upper.getDataValidity() == ctk::DataValidity::ok);
    //=====================================================================
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testInitialValues) {
  std::cout << "*** testInitialValues" << std::endl;

  TestApplication2 app;
  app.upper.sendInitialValue = false;
  ChimeraTK::TestFacility test(false);

  test.runApplication();

  // return channel: upper must start without lower sending anything through the return channel
  CHECK_TIMEOUT(app.upper.mainLoopStarted.try_wait(), 10000);

  // forward channel: lower must not start without upper sending the initial value
  usleep(10000);
  BOOST_CHECK(!app.lower.mainLoopStarted.try_wait());
  app.upper.var = 666;
  app.upper.var.write();
  CHECK_TIMEOUT(app.lower.mainLoopStarted.try_wait(), 10000);
  BOOST_CHECK_EQUAL(app.lower.var, 666);
}

/*********************************************************************************************************************/
