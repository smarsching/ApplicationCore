/*
 * testHierarchyModifyingGroup.cc
 */

#include <chrono>
#include <future>

#define BOOST_TEST_MODULE testHierarchyModifyingGroup

#include <boost/mpl/list.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>

#include "ApplicationCore.h"

using namespace boost::unit_test_framework;
namespace ctk = ChimeraTK;

/*********************************************************************************************************************/

struct TestGroup : public ctk::HierarchyModifyingGroup {
  using ctk::HierarchyModifyingGroup::HierarchyModifyingGroup;
  ctk::ScalarPushInput<int> myVar{this, "myVar", "MV/m", "Descrption"};
};

struct TestApplication : public ctk::Application {
  TestApplication() : Application("testSuite") {}
  ~TestApplication() { shutdown(); }

  void defineConnections() override {}

  struct TestModule : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;

    TestGroup a{this, "VariableGroupLike", "Use like normal VariableGroup", {"TagA"}};
    TestGroup b{this, "/MoveToRoot", "Use like normal VariableGroup with MoveToRoot", {"TagB"}};
    TestGroup c{this, "../oneUp", "Use like normal VariableGroup with oneUp", {"TagC"}};
    TestGroup d{this, "..", "Use like normal VariableGroup with oneUpAndHide", {"TagD"}};
    TestGroup e{this, "local/hierarchy", "Create hierarchy locally", {"TagE"}};
    TestGroup f{this, "/AtRoot/hierarchy", "Create hierarchy at root", {"TagF"}};
    TestGroup g{this, "../oneUp/hierarchy", "Create hierarchy one level up", {"TagG"}};
    TestGroup h{this, "local/very/deep/hierarchy", "Create deep hierarchy locally", {"TagH"}};
    TestGroup i{this, "/root/very/deep/hierarchy", "Create deep hierarchy at root", {"TagI"}};
    TestGroup j{this, "../oneUp/very/deep/hierarchy", "Create deep hierarchy one level up", {"TagJ"}};
    TestGroup k{this, "//extra//slashes////everywhere///", "Extra slashes", {"TagK"}};

    struct ExtraHierarchy : public ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      TestGroup l{this, "../../twoUp", "Two levels up"};
    } extraHierarchy{this, "ExtraHierarchy", "Extra depth", ctk::HierarchyModifier::none, {"TagL"}};

    TestGroup m{this, "hierarchy/with/../dots/../../anywhere/..", "Dots everywhere", {"TagM"}};

    void mainLoop() override {}
  };
  TestModule testModule{this, "TestModule", "The test module"};
};

/*********************************************************************************************************************/
/* Helper for tests: Allows to test the virtual hierarchy quickly, assuming each test case uses a separate tag
 * and each tag/test case has exactly one variable. */
struct TestHelper {
  TestHelper(TestApplication& app, const std::string tag) : root(app.findTag(tag)) {
    current = &root;
    root.dump();
  }

  TestHelper submodule(const std::string& name) {
    BOOST_REQUIRE(current->getSubmoduleList().size() == 1);
    BOOST_CHECK(current->getAccessorList().size() == 0);
    auto child = current->getSubmoduleList().front();
    BOOST_CHECK(child->getName() == name);
    return TestHelper(root, child);
  }

  void accessor(ctk::VariableNetworkNode node) {
    BOOST_CHECK(current->getSubmoduleList().size() == 0);
    BOOST_REQUIRE(current->getAccessorList().size() == 1);
    BOOST_CHECK(current->getAccessorList().front() == node);
  }

 protected:
  TestHelper(ctk::VirtualModule& _root, ctk::Module* _current) : root(_root), current(_current) {}

  ctk::VirtualModule root;
  ctk::Module* current{nullptr};
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(VariableGroupLike) {
  std::cout << "*** VariableGroupLike" << std::endl;
  TestApplication app;
  TestHelper(app, "TagA").submodule("TestModule").submodule("VariableGroupLike").accessor(app.testModule.a.myVar);
}

BOOST_AUTO_TEST_CASE(MoveToRoot) {
  std::cout << "*** MoveToRoot" << std::endl;
  TestApplication app;
  TestHelper(app, "TagB").submodule("MoveToRoot").accessor(app.testModule.b.myVar);
}

BOOST_AUTO_TEST_CASE(oneUp) {
  std::cout << "*** ../oneUp" << std::endl;
  TestApplication app;
  TestHelper(app, "TagC").submodule("oneUp").accessor(app.testModule.c.myVar);
}

BOOST_AUTO_TEST_CASE(dotdot) {
  std::cout << "*** .." << std::endl;
  TestApplication app;
  TestHelper(app, "TagD").accessor(app.testModule.d.myVar);
}

BOOST_AUTO_TEST_CASE(local_hierarchy) {
  std::cout << "*** local/hierarchy" << std::endl;
  TestApplication app;
  TestHelper(app, "TagE")
      .submodule("TestModule")
      .submodule("local")
      .submodule("hierarchy")
      .accessor(app.testModule.e.myVar);
}

BOOST_AUTO_TEST_CASE(AtRoot_hierarchy) {
  std::cout << "*** /AtRoot/hierarchy" << std::endl;
  TestApplication app;
  TestHelper(app, "TagF").submodule("AtRoot").submodule("hierarchy").accessor(app.testModule.f.myVar);
}

BOOST_AUTO_TEST_CASE(oneUp_hierarchy) {
  std::cout << "*** ../oneUp/hierarchy" << std::endl;
  TestApplication app;
  TestHelper(app, "TagG").submodule("oneUp").submodule("hierarchy").accessor(app.testModule.g.myVar);
}

BOOST_AUTO_TEST_CASE(local_very_deep_hierarchy) {
  std::cout << "*** local/very/deep/hierarchy" << std::endl;
  TestApplication app;
  TestHelper(app, "TagH")
      .submodule("TestModule")
      .submodule("local")
      .submodule("very")
      .submodule("deep")
      .submodule("hierarchy")
      .accessor(app.testModule.h.myVar);
}

BOOST_AUTO_TEST_CASE(root_very_deep_hierarchy) {
  std::cout << "*** /root/very/deep/hierarchy" << std::endl;
  TestApplication app;
  TestHelper(app, "TagI")
      .submodule("root")
      .submodule("very")
      .submodule("deep")
      .submodule("hierarchy")
      .accessor(app.testModule.i.myVar);
}

BOOST_AUTO_TEST_CASE(oneUp_very_deep_hierarchy) {
  std::cout << "*** ../oneUp/very/deep/hierarchy" << std::endl;
  TestApplication app;
  TestHelper(app, "TagJ")
      .submodule("oneUp")
      .submodule("very")
      .submodule("deep")
      .submodule("hierarchy")
      .accessor(app.testModule.j.myVar);
}

BOOST_AUTO_TEST_CASE(extra_slashes_everywhere) {
  std::cout << "*** //extra//slashes////everywhere///" << std::endl;
  TestApplication app;
  TestHelper(app, "TagK")
      .submodule("extra")
      .submodule("slashes")
      .submodule("everywhere")
      .accessor(app.testModule.k.myVar);
}

BOOST_AUTO_TEST_CASE(twoUp) {
  std::cout << "*** twoUp" << std::endl;
  TestApplication app;
  TestHelper(app, "TagL").submodule("twoUp").accessor(app.testModule.extraHierarchy.l.myVar);
}

BOOST_AUTO_TEST_CASE(hierarchy_with_dots_anywhere) {
  std::cout << "*** hierarchy/with/../dots/../../anywhere/.." << std::endl;
  TestApplication app;
  TestHelper(app, "TagM").submodule("TestModule").accessor(app.testModule.m.myVar);
}

/*********************************************************************************************************************/

struct TestApplication_empty : public ctk::Application {
  TestApplication_empty() : Application("testSuite") {}
  ~TestApplication_empty() { shutdown(); }
  void defineConnections() override {}

  struct TestModule : ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;
    void mainLoop() override {}
  } testModule{this, "TestModule", "The test module"};

  struct TestModuleGroup : ctk::ModuleGroup {
    using ModuleGroup::ModuleGroup;
  } testModuleGroup{this, "TestModuleGroup", "The test module group"};
};

BOOST_AUTO_TEST_CASE(ownership_exception) {
  std::cout << "*** ownership_exception" << std::endl;
  TestApplication_empty app;
  BOOST_CHECK_THROW(TestGroup tg(&app, "TestGroup", "Cannot be directly owned by Application"), ctk::logic_error);
  BOOST_CHECK_THROW(
      TestGroup tg(&app.testModuleGroup, "TestGroup", "Cannot be directly owned by ModuleGroup"), ctk::logic_error);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(bad_path_exception) {
  std::cout << "*** bad_path_exception" << std::endl;
  TestApplication_empty app;
  BOOST_CHECK_THROW(TestGroup tg(&app.testModule, "/../cannot/work", "This is not allowed"), ctk::logic_error);
  BOOST_CHECK_THROW(TestGroup tg(&app.testModule, "/..", "This is not allowed either"), ctk::logic_error);

  // this only failes when it is actaully used
  TestGroup tg(&app.testModule, "/somthing/less/../../../obvious", "This is also not allowed");
  BOOST_CHECK_THROW(app.findTag(".*"), ctk::logic_error);
}

/*********************************************************************************************************************/
