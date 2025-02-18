// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/EnableXMLGenerator.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>

namespace ctk = ChimeraTK;

struct Controller : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  ctk::ScalarPollInput<double> sp{this, "temperatureSetpoint", "degC", "Description"};
  ctk::ScalarPushInput<double> rb{this, "temperatureReadback", "degC", "..."};
  ctk::ScalarOutput<double> cur{this, "heatingCurrent", "mA", "..."};

  void mainLoop() {
    const double gain = 100.0;
    while(true) {
      readAll(); // waits until rb updated, then reads sp

      cur = gain * (sp - rb);
      writeAll(); // writes any outputs
    }
  }
};

struct ExampleApp : public ctk::Application {
  ExampleApp() : Application("demoApp2") {}
  ~ExampleApp() { shutdown(); }

  // We can pick any name for the module. "Oven" is what we want to see in the CS
  Controller controller{this, "Oven", "The controller of the oven"};

  ctk::PeriodicTrigger timer{this, "Timer", "Periodic timer for the controller", 1000};

  ctk::DeviceModule oven{this, "oven", &initialiseOven};
  ctk::ControlSystemModule cs;

  void defineConnections();
  static void initialiseOven(ChimeraTK::DeviceModule* oven);
};
static ExampleApp theExampleApp;

void ExampleApp::defineConnections() {
  ChimeraTK::setDMapFilePath("example2.dmap");

  // Connect everything to the CS (except for the device, which is special)
  findTag(".*").connectTo(cs);
  // Connect device's "heater" to "Oven" in the CS, and use timer.tick as trigger
  oven["heater"].connectTo(cs["Oven"], timer.tick);
}

void ExampleApp::initialiseOven(ChimeraTK::DeviceModule* oven) {
  // set the gain factors for the voltage monitoring ADCs
  oven->device.write<uint32_t>("/settings/supplyVoltageAdcGains", {20, 1, 1, 1});
}
