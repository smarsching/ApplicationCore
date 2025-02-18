// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/ConfigReader.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>

namespace ctk = ChimeraTK;

struct Controller : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPollInput<double> sp{this, "temperatureSetpoint", "degC", "Description", {"CS"}};
  ctk::ScalarPushInput<double> rb{this, "temperatureReadback", "degC", "...", {"DEV", "CS"}};
  ctk::ScalarOutput<double> cur{this, "heatingCurrent", "mA", "...", {"DEV"}};

  void mainLoop() {
    const double gain = 100.0;
    while(true) {
      readAll(); // waits until rb updated, then reads sp

      cur = gain * (sp - rb);
      writeAll(); // writes any outputs
    }
  }
};

struct Automation : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPollInput<double> opSp{this, "operatorSetpoint", "degC", "...", {"CS"}};
  ctk::ScalarOutput<double> actSp{this, "temperatureSetpoint", "degC", "...", {"Controller"}};
  ctk::ScalarPushInput<uint64_t> trigger{this, "trigger", "", "..."};

  void mainLoop() {
    const double maxStep = 0.1;
    while(true) {
      readAll(); // waits until trigger received, then read opSp
      actSp += std::max(std::min(opSp - actSp, maxStep), -maxStep);
      writeAll();
    }
  }
};

struct ExampleApp : public ctk::Application {
  ExampleApp() : Application("exampleApp2a") {}
  ~ExampleApp() { shutdown(); }

  ctk::ConfigReader config{this, "config", "demoApp2a.xml"};

  Controller controller{this, "Controller", "The Controller"};
  Automation automation;

  ctk::PeriodicTrigger timer{this, "Timer", "Periodic timer for the controller", 1000};

  ctk::DeviceModule oven{this, "oven"};
  ctk::ControlSystemModule cs;

  void defineConnections();
};
static ExampleApp theExampleApp;

void ExampleApp::defineConnections() {
  ChimeraTK::setDMapFilePath("example2.dmap");

  config.connectTo(cs["Configuration"]);
  if(config.get<int>("enableAutomation")) {
    automation = Automation(this, "Automation", "Slow setpoint ramping algorithm");
    automation.findTag("Controller").connectTo(controller);
    timer.tick >> automation.trigger;
  }

  controller.findTag("DEV").connectTo(oven["heater"], timer.tick);
  findTag("CS").connectTo(cs);
}
