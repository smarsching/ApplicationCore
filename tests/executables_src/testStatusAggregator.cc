// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Application.h"
#include "ModuleGroup.h"
#include "StatusAggregator.h"
#include "TestFacility.h"

#define BOOST_NO_EXCEPTIONS
#define BOOST_TEST_MODULE testStatusAggregator
#include <boost/test/included/unit_test.hpp>
#undef BOOST_NO_EXCEPTIONS

using namespace boost::unit_test_framework;

namespace ctk = ChimeraTK;

/**********************************************************************************************************************/

struct StatusGenerator : ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::StatusOutput status{this, getName(), ""};
  void mainLoop() override {}
};

/**********************************************************************************************************************/

struct TestApplication : ctk::Application {
  TestApplication() : Application("testApp") {}
  ~TestApplication() override { shutdown(); }

  StatusGenerator s{this, "s", "Status", ctk::HierarchyModifier::hideThis};

  struct OuterGroup : ctk::ModuleGroup {
    using ctk::ModuleGroup::ModuleGroup;

    StatusGenerator s1{this, "s1", "Status 1", ctk::HierarchyModifier::hideThis};
    StatusGenerator s2{this, "s2", "Status 2", ctk::HierarchyModifier::hideThis};

    struct InnerGroup : ctk::ModuleGroup {
      using ctk::ModuleGroup::ModuleGroup;
      StatusGenerator s{this, "s", "Status", ctk::HierarchyModifier::hideThis};
      StatusGenerator deep{this, "deep", "Status"};
    };
    InnerGroup innerGroup1{this, "InnerGroup1", ""};
    InnerGroup innerGroup2{this, "InnerGroup2", ""};

  } outerGroup{this, "OuterGroup", ""};

  ctk::StatusAggregator aggregator{
      this, "Aggregated/status", "aggregated status description", ctk::StatusAggregator::PriorityMode::fwko};
};

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testSingleNoTags) {
  std::cout << "testSingleNoTags" << std::endl;

  TestApplication app;
  ctk::TestFacility test;

  auto status = test.getScalar<int>("/Aggregated/status");

  // write initial values
  app.s.status = ctk::StatusOutput::Status::OFF;
  app.s.status.write();
  app.outerGroup.s1.status = ctk::StatusOutput::Status::OFF;
  app.outerGroup.s1.status.write();
  app.outerGroup.s2.status = ctk::StatusOutput::Status::OFF;
  app.outerGroup.s2.status.write();
  app.outerGroup.innerGroup1.s.status = ctk::StatusOutput::Status::OFF;
  app.outerGroup.innerGroup1.s.status.write();
  app.outerGroup.innerGroup1.deep.status = ctk::StatusOutput::Status::OFF;
  app.outerGroup.innerGroup1.deep.status.write();
  app.outerGroup.innerGroup2.s.status = ctk::StatusOutput::Status::OFF;
  app.outerGroup.innerGroup2.s.status.write();
  app.outerGroup.innerGroup2.deep.status = ctk::StatusOutput::Status::OFF;
  app.outerGroup.innerGroup2.deep.status.write();

  test.runApplication();

  // check that statuses on different levels are correctly aggregated
  auto check = [&](auto& var) {
    var = ctk::StatusOutput::Status::OK;
    var.write();
    test.stepApplication();
    BOOST_CHECK(status.readNonBlocking() == true);
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
    var = ctk::StatusOutput::Status::OFF;
    var.write();
    test.stepApplication();
    BOOST_CHECK(status.readNonBlocking() == true);
    BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));
  };
  check(app.s.status);
  check(app.outerGroup.s1.status);
  check(app.outerGroup.s2.status);
  check(app.outerGroup.innerGroup1.s.status);
  check(app.outerGroup.innerGroup1.deep.status);
  check(app.outerGroup.innerGroup2.s.status);
  check(app.outerGroup.innerGroup2.deep.status);
}

/**********************************************************************************************************************/

struct TestPrioApplication : ctk::Application {
  TestPrioApplication() : Application("testApp") {}
  ~TestPrioApplication() override { shutdown(); }

  StatusGenerator s1{this, "s1", "Status 1", ctk::HierarchyModifier::hideThis};
  StatusGenerator s2{this, "s2", "Status 2", ctk::HierarchyModifier::hideThis};

  ctk::StatusAggregator aggregator;
};

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testPriorities) {
  std::cout << "testPriorities" << std::endl;

  // Define repeated check for a given priority mode
  auto check = [&](ctk::StatusAggregator::PriorityMode mode, auto prio0, auto prio1, auto prio2, auto prio3,
                   bool warnMixed01 = false) {
    TestPrioApplication app;
    app.aggregator = ctk::StatusAggregator{&app, "Aggregated/status", "aggregated status description", mode};

    ctk::TestFacility test;

    auto status = test.getScalar<int>("/Aggregated/status");

    // write initial values (all in lowest prio value)
    app.s1.status = prio0;
    app.s1.status.write();
    app.s2.status = prio0;
    app.s2.status.write();

    test.runApplication();

    // check initial value
    status.readNonBlocking(); // do not check return value, as it will only be written when changed
    BOOST_CHECK_EQUAL(int(status), int(prio0));

    // define repeated check to test all combinations of two given values with different priority
    auto subcheck = [&](auto lower, auto higher, bool warnMixed = false) {
      std::cout << int(lower) << " vs. " << int(higher) << std::endl;
      app.s1.status = lower;
      app.s1.status.write();
      app.s2.status = lower;
      app.s2.status.write();
      test.stepApplication();
      status.readLatest();
      BOOST_CHECK_EQUAL(int(status), int(lower));
      app.s1.status = lower;
      app.s1.status.write();
      app.s2.status = higher;
      app.s2.status.write();
      test.stepApplication();
      status.readLatest();
      if(!warnMixed) {
        BOOST_CHECK_EQUAL(int(status), int(higher));
      }
      else {
        BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::WARNING));
      }
      app.s1.status = higher;
      app.s1.status.write();
      app.s2.status = lower;
      app.s2.status.write();
      test.stepApplication();
      status.readLatest();
      if(!warnMixed) {
        BOOST_CHECK_EQUAL(int(status), int(higher));
      }
      else {
        BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::WARNING));
      }
      app.s1.status = higher;
      app.s1.status.write();
      app.s2.status = higher;
      app.s2.status.write();
      test.stepApplication();
      status.readLatest();
      BOOST_CHECK_EQUAL(int(status), int(higher));
    };

    // all prios against each other
    subcheck(prio0, prio1, warnMixed01);
    subcheck(prio0, prio2);
    subcheck(prio0, prio3);
    subcheck(prio1, prio2);
    subcheck(prio1, prio3);
    subcheck(prio2, prio3);
  };

  // check all priority modes
  std::cout << "PriorityMode::fwko" << std::endl;
  check(ctk::StatusAggregator::PriorityMode::fwko, ctk::StatusOutput::Status::OFF, ctk::StatusOutput::Status::OK,
      ctk::StatusOutput::Status::WARNING, ctk::StatusOutput::Status::FAULT);
  std::cout << "PriorityMode::fwok" << std::endl;
  check(ctk::StatusAggregator::PriorityMode::fwok, ctk::StatusOutput::Status::OK, ctk::StatusOutput::Status::OFF,
      ctk::StatusOutput::Status::WARNING, ctk::StatusOutput::Status::FAULT);
  std::cout << "PriorityMode::ofwk" << std::endl;
  check(ctk::StatusAggregator::PriorityMode::ofwk, ctk::StatusOutput::Status::OK, ctk::StatusOutput::Status::WARNING,
      ctk::StatusOutput::Status::FAULT, ctk::StatusOutput::Status::OFF);
  std::cout << "PriorityMode::fw_warn_mixed" << std::endl;
  check(ctk::StatusAggregator::PriorityMode::fw_warn_mixed, ctk::StatusOutput::Status::OFF,
      ctk::StatusOutput::Status::OK, ctk::StatusOutput::Status::WARNING, ctk::StatusOutput::Status::FAULT, true);
}

/**********************************************************************************************************************/

struct TestApplication2Levels : ctk::Application {
  TestApplication2Levels() : Application("testApp") {}
  ~TestApplication2Levels() override { shutdown(); }

  StatusGenerator s{this, "s", "Status", ctk::HierarchyModifier::hideThis};

  struct OuterGroup : ctk::ModuleGroup {
    using ctk::ModuleGroup::ModuleGroup;

    StatusGenerator s1{this, "s1", "Status 1", ctk::HierarchyModifier::hideThis};
    StatusGenerator s2{this, "s2", "Status 2", ctk::HierarchyModifier::hideThis};

    ctk::StatusAggregator extraAggregator{
        this, "/Aggregated/extraStatus", "aggregated status description", ctk::StatusAggregator::PriorityMode::ofwk};

  } outerGroup{this, "OuterGroup", ""};

  ctk::StatusAggregator aggregator{
      this, "Aggregated/status", "aggregated status description", ctk::StatusAggregator::PriorityMode::fwko};
};

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testTwoLevels) {
  std::cout << "testTwoLevels" << std::endl;
  TestApplication2Levels app;

  ctk::TestFacility test;

  auto status = test.getScalar<int>("/Aggregated/status");
  auto extraStatus = test.getScalar<int>("/Aggregated/extraStatus");

  // write initial values
  app.s.status = ctk::StatusOutput::Status::OFF;
  app.s.status.write();
  app.outerGroup.s2.status = ctk::StatusOutput::Status::OFF;
  app.outerGroup.s2.status.write();
  // Set one of the inputs for the extraAggregator to fault, which has no effect, since one other is OFF which is
  // prioritised. If the top-level aggregator would wrongly aggregate this input directly, it would go to FAULT.
  app.outerGroup.s1.status = ctk::StatusOutput::Status::FAULT;
  app.outerGroup.s1.status.write();

  test.runApplication();

  // check the initial values
  extraStatus.readLatest();
  BOOST_CHECK_EQUAL(int(extraStatus), int(ctk::StatusOutput::Status::OFF));
  status.readLatest();
  BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));

  // change status which goes directly into the upper aggregator
  app.s.status = ctk::StatusOutput::Status::OK;
  app.s.status.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
  app.s.status = ctk::StatusOutput::Status::OFF;
  app.s.status.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));

  // change status which goes into the lower aggregator (then the FAULT of s1 will win)
  app.outerGroup.s2.status = ctk::StatusOutput::Status::OK;
  app.outerGroup.s2.status.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::FAULT));
  app.outerGroup.s2.status = ctk::StatusOutput::Status::OFF;
  app.outerGroup.s2.status.write();
  test.stepApplication();
  status.readLatest();
  BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OFF));
}

/**********************************************************************************************************************/

struct TestApplicationTags : ctk::Application {
  TestApplicationTags() : Application("testApp") {}
  ~TestApplicationTags() override { shutdown(); }

  struct OuterGroup : ctk::ModuleGroup {
    using ctk::ModuleGroup::ModuleGroup;

    StatusGenerator sA{this, "sA", "Status 1", ctk::HierarchyModifier::hideThis, {"A"}};
    StatusGenerator sAB{this, "sAB", "Status 2", ctk::HierarchyModifier::hideThis, {"A", "B"}};

    ctk::StatusAggregator aggregateA{
        this, "aggregateA", "aggregated status description", ctk::StatusAggregator::PriorityMode::fwko, {"A"}};
    ctk::StatusAggregator aggregateB{this, "aggregateB", "aggregated status description",
        ctk::StatusAggregator::PriorityMode::fwko, {"B"}, {"A"}}; // the "A" tag should be ignored by other aggregators

  } group{this, "Group", ""};

  // Use other priority mode here to make sure only the aggregators are aggregated, not the generators
  ctk::StatusAggregator aggregateA{
      this, "aggregateA", "aggregated status description", ctk::StatusAggregator::PriorityMode::ofwk, {"A"}};
  ctk::StatusAggregator aggregateB{
      this, "aggregateB", "aggregated status description", ctk::StatusAggregator::PriorityMode::ofwk, {"B"}};
  ctk::StatusAggregator aggregateAll{
      this, "aggregateAll", "aggregated status description", ctk::StatusAggregator::PriorityMode::fw_warn_mixed};
};

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testTags) {
  std::cout << "testTags" << std::endl;

  TestApplicationTags app;
  ctk::TestFacility test;

  auto aggregateA = test.getScalar<int>("/aggregateA");
  auto aggregateB = test.getScalar<int>("/aggregateB");
  auto aggregateAll = test.getScalar<int>("/aggregateAll");
  auto Group_aggregateA = test.getScalar<int>("/Group/aggregateA");
  auto Group_aggregateB = test.getScalar<int>("/Group/aggregateB");

  // write initial values
  app.group.sA.status = ctk::StatusOutput::Status::WARNING;
  app.group.sA.status.write();
  app.group.sAB.status = ctk::StatusOutput::Status::OFF;
  app.group.sAB.status.write();

  test.runApplication();

  // check initial values
  aggregateA.readLatest();
  aggregateB.readLatest();
  aggregateAll.readLatest();
  Group_aggregateA.readLatest();
  Group_aggregateB.readLatest();
  BOOST_CHECK_EQUAL(int(aggregateA), int(ctk::StatusOutput::Status::WARNING));
  BOOST_CHECK_EQUAL(int(aggregateB), int(ctk::StatusOutput::Status::OFF));
  BOOST_CHECK_EQUAL(int(aggregateAll), int(ctk::StatusOutput::Status::WARNING));
  BOOST_CHECK_EQUAL(int(Group_aggregateA), int(ctk::StatusOutput::Status::WARNING));
  BOOST_CHECK_EQUAL(int(Group_aggregateB), int(ctk::StatusOutput::Status::OFF));

  app.group.sAB.status = ctk::StatusOutput::Status::FAULT;
  app.group.sAB.status.write();

  test.stepApplication();

  // change value tagged with 'A' and 'B', affecting all aggregators to highest priority value, so it is visible
  // everywhere
  aggregateA.readLatest();
  aggregateB.readLatest();
  aggregateAll.readLatest();
  Group_aggregateA.readLatest();
  Group_aggregateB.readLatest();
  BOOST_CHECK_EQUAL(int(aggregateA), int(ctk::StatusOutput::Status::FAULT));
  BOOST_CHECK_EQUAL(int(aggregateB), int(ctk::StatusOutput::Status::FAULT));
  BOOST_CHECK_EQUAL(int(aggregateAll), int(ctk::StatusOutput::Status::FAULT));
  BOOST_CHECK_EQUAL(int(Group_aggregateA), int(ctk::StatusOutput::Status::FAULT));
  BOOST_CHECK_EQUAL(int(Group_aggregateB), int(ctk::StatusOutput::Status::FAULT));
}

/**********************************************************************************************************************/

// test behavior for status+string:
// test that status aggregator always has a message output and hands it over to next status aggregator
BOOST_AUTO_TEST_CASE(testStatusMessage) {
  std::cout << "testStatusMessage" << std::endl;
  TestApplication2Levels app;

  ctk::TestFacility test;

  auto status = test.getScalar<int>("/Aggregated/status");
  auto statusMessage = test.getScalar<std::string>("/Aggregated/status_message");
  auto innerStatus = test.getScalar<int>("/Aggregated/extraStatus");
  auto innerStatusMessage = test.getScalar<std::string>("/Aggregated/extraStatus_message");

  // write initial values
  app.s.status = ctk::StatusOutput::Status::OK;
  app.s.status.write();
  app.outerGroup.s1.status = ctk::StatusOutput::Status::OK;
  app.outerGroup.s1.status.write();
  app.outerGroup.s2.status = ctk::StatusOutput::Status::OK;
  app.outerGroup.s2.status.write();

  test.runApplication();

  // check the initial values
  innerStatus.readLatest();
  BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::OK));
  innerStatusMessage.readLatest();
  BOOST_CHECK_EQUAL(std::string(innerStatusMessage), "");
  status.readLatest();
  BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::OK));
  statusMessage.readLatest();
  BOOST_CHECK_EQUAL(std::string(statusMessage), "");

  app.outerGroup.s2.status = ctk::StatusOutput::Status::FAULT;
  app.outerGroup.s2.status.write();
  test.stepApplication();
  status.readLatest();
  statusMessage.readLatest();
  innerStatus.readLatest();
  innerStatusMessage.readLatest();
  BOOST_CHECK_EQUAL(int(status), int(ctk::StatusOutput::Status::FAULT));
  const char* faultString = "/testApp/OuterGroup/s2/s2 switched to FAULT";
  BOOST_CHECK_EQUAL(std::string(statusMessage), faultString);
  BOOST_CHECK_EQUAL(int(innerStatus), int(ctk::StatusOutput::Status::FAULT));
  BOOST_CHECK_EQUAL(std::string(innerStatusMessage), faultString);
}

/**********************************************************************************************************************/
