// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testConnectTo

#include "Application.h"
#include "ApplicationModule.h"
#include "ModuleGroup.h"
#include "ScalarAccessor.h"
#include "TestFacility.h"
#include "VariableGroup.h"
#include "VirtualModule.h"

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

/*********************************************************************************************************************/
/* Build first hierarchy */

struct FirstHierarchy : ctk::ModuleGroup {
  using ctk::ModuleGroup::ModuleGroup;

  struct TestModule : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct VarGroup : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<int> varA{this, "varA", "MV/m", "Desc"};
      ctk::ScalarPushInput<int> varB{this, "varB", "MV/m", "Desc"};
      ctk::ScalarOutput<int> varC{this, "varC", "MV/m", "Desc"};
    } varGroup{this, "VarGroup", "A group"};

    ctk::ScalarPushInput<int> varA{this, "varA", "MV/m", "Desc"};
    ctk::ScalarOutput<int> varX{this, "varX", "MV/m", "Desc"};

    void prepare() override { writeAll(); /*send initial values*/ }
    void mainLoop() override {}
  } testModule{this, "TestModule", ""};

  struct SecondModule : ctk::ApplicationModule {
    SecondModule(EntityOwner* owner, const std::string& name, const std::string& description)
    : ctk::ApplicationModule(owner, name, description) {
      for(size_t i = 0; i < 22; ++i) myVec.emplace_back(this, "Var" + std::to_string(i), "Unit", "Foo");
    }
    SecondModule() { throw; } // work around for gcc bug: constructor must be present but is unused

    std::vector<ctk::ScalarPushInput<int>> myVec;

    void prepare() override { writeAll(); /*send initial values*/ }
    void mainLoop() override {}

  } secondModule{this, "SecondModule", ""};
};

/*********************************************************************************************************************/
/* Build second hierarchy */

struct SecondHierarchy : ctk::ModuleGroup {
  using ctk::ModuleGroup::ModuleGroup;

  struct TestModule : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    struct VarGroup : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarOutput<int> varA{this, "varA", "MV/m", "Desc"};
      ctk::ScalarPushInput<int> varC{this, "varC", "MV/m", "Desc"};
      ctk::ScalarPushInput<int> varD{this, "varD", "MV/m", "Desc"};
    } varGroup{this, "VarGroup", "A group"};

    ctk::ScalarPushInput<int> extraVar{this, "extraVar", "MV/m", "Desc"};
    ctk::ScalarOutput<int> varA{this, "varA", "MV/m", "Desc"};

    struct EliminatedGroup : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarPushInput<int> varX{this, "varX", "MV/m", "Desc"};
      struct VarGroup : ctk::VariableGroup {
        using ctk::VariableGroup::VariableGroup;
        ctk::ScalarOutput<int> varB{this, "varB", "MV/m", "Desc"};
      } varGroup{this, "VarGroup", "This group shall be merged with testModule.varGroup in connectTo()"};
    } eliminatedGroup{
        this, "eliminatedGroup", "A group whose hierarchy gets eliminated", ctk::HierarchyModifier::hideThis};

    void prepare() override { writeAll(); /*send initial values*/ }
    void mainLoop() override {}
  } testModule{this, "TestModule", ""};

  struct SecondModule : ctk::ApplicationModule {
    SecondModule(EntityOwner* owner, const std::string& name, const std::string& description)
    : ctk::ApplicationModule(owner, name, description) {
      for(size_t i = 0; i < 22; ++i) myVec.emplace_back(this, "Var" + std::to_string(i), "Unit", "Foo");
    }
    SecondModule() { throw; } // work around for gcc bug: constructor must be present but is unused

    struct ExtraGroup : ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      ctk::ScalarOutput<int> varA{this, "varA", "MV/m", "Desc"};
    } extraGroup{this, "ExtraGroup", "A group"};

    std::vector<ctk::ScalarOutput<int>> myVec;

    void prepare() override { writeAll(); /*send initial values*/ }
    void mainLoop() override {}

  } secondModule{this, "SecondModule", ""};
};

/*********************************************************************************************************************/
/* dummy application */

struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() { shutdown(); }

  using Application::makeConnections; // we call makeConnections() manually in
                                      // the tests to catch exceptions etc.
  void defineConnections() {}         // the setup is done in the tests

  FirstHierarchy first{this, "first", "The test module"};
  SecondHierarchy second{this, "second", "The test module"};
};

/*********************************************************************************************************************/
/* test connectTo() */

BOOST_AUTO_TEST_CASE(testConnectTo) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testConnectTo" << std::endl;

  TestApplication app;
  app.first.connectTo(app.second);

  ctk::TestFacility test;
  test.runApplication();

  app.second.testModule.varGroup.varA = 1;
  app.second.testModule.eliminatedGroup.varGroup.varB = 2;
  app.first.testModule.varGroup.varC = 3;
  app.second.testModule.varA = 4;
  app.first.testModule.varX = 5;
  for(int i = 0; i < 22; ++i) {
    app.second.secondModule.myVec[i] = 6 + i;
  }

  app.first.testModule.writeAll();
  app.first.secondModule.writeAll();
  app.second.testModule.writeAll();
  app.second.secondModule.writeAll();
  app.first.testModule.readAllLatest();
  app.first.secondModule.readAllLatest();
  app.second.testModule.readAllLatest();
  app.second.secondModule.readAllLatest();

  BOOST_CHECK_EQUAL((int)app.first.testModule.varGroup.varA, 1);
  BOOST_CHECK_EQUAL((int)app.first.testModule.varGroup.varB, 2);
  BOOST_CHECK_EQUAL((int)app.second.testModule.varGroup.varC, 3);
  BOOST_CHECK_EQUAL((int)app.first.testModule.varA, 4);
  BOOST_CHECK_EQUAL((int)app.second.testModule.eliminatedGroup.varX, 5);
  for(int i = 0; i < 22; ++i) {
    BOOST_CHECK_EQUAL((int)app.first.secondModule.myVec[i], 6 + i);
  }
}

/*********************************************************************************************************************/
/* check if makeing the same connection twice does not fail */

BOOST_AUTO_TEST_CASE(testConnectTwice) {
  std::cout << "***************************************************************"
               "******************************************************"
            << std::endl;
  std::cout << "==> testConnectTwice" << std::endl;

  TestApplication app;
  app.second.testModule.varA >> app.first.testModule.varA;
  app.first.connectTo(app.second);

  ctk::TestFacility test;
  test.runApplication();

  app.second.testModule.varGroup.varA = 1;
  app.second.testModule.eliminatedGroup.varGroup.varB = 2;
  app.first.testModule.varGroup.varC = 3;
  app.second.testModule.varA = 4;
  app.first.testModule.varX = 5;
  for(int i = 0; i < 22; ++i) {
    app.second.secondModule.myVec[i] = 6 + i;
  }

  app.first.testModule.writeAll();
  app.first.secondModule.writeAll();
  app.second.testModule.writeAll();
  app.second.secondModule.writeAll();
  app.first.testModule.readAllLatest();
  app.first.secondModule.readAllLatest();
  app.second.testModule.readAllLatest();
  app.second.secondModule.readAllLatest();

  BOOST_CHECK_EQUAL((int)app.first.testModule.varGroup.varA, 1);
  BOOST_CHECK_EQUAL((int)app.first.testModule.varGroup.varB, 2);
  BOOST_CHECK_EQUAL((int)app.second.testModule.varGroup.varC, 3);
  BOOST_CHECK_EQUAL((int)app.first.testModule.varA, 4);
  BOOST_CHECK_EQUAL((int)app.second.testModule.eliminatedGroup.varX, 5);
  for(int i = 0; i < 22; ++i) {
    BOOST_CHECK_EQUAL((int)app.first.secondModule.myVec[i], 6 + i);
  }
}
