// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "ApplicationCore.h"
#include "HierarchyModifyingGroup.h"

#include <chrono>

namespace ChimeraTK {

  /**
   * Simple periodic trigger that fires a variable once per second.
   * After configurable number of seconds it will wrap around
   */
  struct PeriodicTrigger : public ApplicationModule {
    /** Constructor. In addition to the usual arguments of an ApplicationModule,
     * the default timeout value is specified.
     *  This value is used as a timeout if the timeout value is set to 0. The
     * timeout value is in milliseconds.
     * @param periodName, tickName Qualified names for the period and the tick variable.
     *  You can just give a variable name, a relative or an absolute path.*/
    PeriodicTrigger(EntityOwner* owner, const std::string& name, const std::string& description,
        const uint32_t defaultPeriod = 1000, bool eliminateHierarchy = false,
        const std::unordered_set<std::string>& tags = {}, std::string periodName = "period",
        std::string tickName = "tick")
    : ApplicationModule(owner, name, description, eliminateHierarchy, tags),
      hierarchyModifiedPeriod(
          this, periodName, "ms", "period in milliseconds. The trigger is sent once per the specified duration."),
      hierarchyModifiedTick(this, tickName, "", "Timer tick. Counts the trigger number starting from 0."),
      period(hierarchyModifiedPeriod.value), tick(hierarchyModifiedTick.value), defaultPeriod_(defaultPeriod) {}

    /** Move constructor */
    PeriodicTrigger(PeriodicTrigger&& other)
    : ApplicationModule(std::move(other)), hierarchyModifiedPeriod(std::move(other.hierarchyModifiedPeriod)),
      hierarchyModifiedTick(std::move(other.hierarchyModifiedTick)), period(hierarchyModifiedPeriod.value),
      tick(hierarchyModifiedTick.value), defaultPeriod_(other.defaultPeriod_) {}

    /** Move assignment */
    PeriodicTrigger& operator=(PeriodicTrigger&& rhs) {
      this->~PeriodicTrigger();
      new(this) PeriodicTrigger(std::move(rhs));
      return *this;
    }

    // The references period and tick allow to directly access the input and output.
    // This serves two purposes:
    // 1. Avoid having to call the .value of ModifyHierarchy each time.
    // 2. Keep the code API compatible with the previous version which did not have the ModifyHierarchy but directly
    //    had a ScalarOutput named tick. Some older code which does not connect via the CS name yet but uses direct
    //    connection might break otherwise.
    ModifyHierarchy<ScalarPollInput<uint32_t>> hierarchyModifiedPeriod;
    ModifyHierarchy<ScalarOutput<uint64_t>> hierarchyModifiedTick;
    ScalarPollInput<uint32_t>& period;
    ScalarOutput<uint64_t>& tick;

    void prepare() override {
      setCurrentVersionNumber({});
      tick.write(); // send initial value
    }

    void sendTrigger() {
      setCurrentVersionNumber({});
      ++tick;
      tick.write();
    }

    void mainLoop() override {
      if(Application::getInstance().isTestableModeEnabled()) {
        return;
      }
      tick = 0;
      std::chrono::time_point<std::chrono::steady_clock> t = std::chrono::steady_clock::now();

      while(true) {
        period.read();
        if(period == 0) {
          // set receiving end of timeout. Will only be overwritten if there is
          // new data.
          period = defaultPeriod_;
        }
        t += std::chrono::milliseconds(static_cast<uint32_t>(period));
        boost::this_thread::interruption_point();
        std::this_thread::sleep_until(t);

        sendTrigger();
      }
    }

   private:
    uint32_t defaultPeriod_;
  };
} // namespace ChimeraTK
