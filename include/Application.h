// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "EntityOwner.h"
#include "Flags.h"
#include "InternalModule.h"
#include "VariableNetwork.h"

#include <ChimeraTK/ControlSystemAdapter/ApplicationBase.h>
#include <ChimeraTK/DeviceBackend.h>

#include <atomic>
#include <mutex>

namespace ChimeraTK {

  /*********************************************************************************************************************/

  class Module;
  class AccessorBase;
  class VariableNetwork;
  class TriggerFanOut;
  class TestFacility;
  class DeviceModule;
  class ApplicationModule;

  template<typename UserType>
  class Accessor;
  template<typename UserType>
  class FanOut;
  template<typename UserType>
  class ConsumingFanOut;

  /*********************************************************************************************************************/

  class Application : public ApplicationBase, public EntityOwner {
   public:
    /** The constructor takes the application name as an argument. The name must
     * have a non-zero length and must not contain any spaces or special
     * characters. Use only alphanumeric characters and underscores. */
    Application(const std::string& name);

    using ApplicationBase::getName;

    /** This will remove the global pointer to the instance and allows creating
     * another instance afterwards. This is mostly useful for writing tests, as it
     * allows to run several applications sequentially in the same executable.
     * Note that any ApplicationModules etc. owned by this Application are no
     * longer
     *  valid after destroying the Application and must be destroyed as well (or
     * at least no longer used). */
    void shutdown() override;

    /** Define the connections between process variables. Can be implemented by the application developer. The default
     *  implementation will connect the entire application with the control system (virtual hierarchy). */
    virtual void defineConnections();

    void initialise() override;

    void optimiseUnmappedVariables(const std::set<std::string>& names) override;

    void run() override;

    /** Instead of running the application, just initialise it and output the
     * published variables to an XML file. */
    void generateXML();

    /** Output the connections requested in the initialise() function to
     * std::cout. This may be done also before
     *  makeConnections() has been called. */
    void dumpConnections(std::ostream& stream = std::cout);

    /** Create Graphviz dot graph and write to file. The graph will contain the
     * connections made in the initilise() function. @see dumpConnections */
    void dumpConnectionGraph(const std::string& filename = {"connections-graph.dot"});

    /** Create Graphviz dot graph representing the connections between the modules, and write to file.*/
    void dumpModuleConnectionGraph(const std::string& filename = {"module-connections-graph.dot"}) const;

    /** Enable warning about unconnected variables. This can be helpful to
     * identify missing connections but is
     *  disabled by default since it may often be very noisy. */
    void warnUnconnectedVariables() { enableUnconnectedVariablesWarning = true; }

    /** Obtain instance of the application. Will throw an exception if called
     * before the instance has been created by the control system adapter, or if
     * the instance is not based on the Application class. */
    static Application& getInstance();

    /** Enable the testable mode. This allows to pause and resume the application
     * for testing purposes using the functions pauseApplication() and
     * resumeApplication(). The application will start in paused state.
     *
     *  This function must be called before the application is initialised (i.e.
     * before the call to initialise()).
     *
     *  Note: Enabling the testable mode will have a singificant impact on the
     * performance, since it will prevent any module threads to run at the same
     * time! */
    void enableTestableMode();

    /**
     * Returns true if application is in testable mode else returns
     * false.
     **/
    bool isTestableModeEnabled() { return testableMode; }

    /**
     * Check whether data has been sent to the application so stepApplication() can be called.
     */
    bool canStepApplication() const;

    /** Resume the application until all application threads are stuck in a blocking read operation. Works only when
     *  the testable mode was enabled.
     *  The optional argument controls whether to wait as well for devices to be completely (re-)initialised. Disabling
     *  this behavior allows to test proper response to runtime exceptions. */
    void stepApplication(bool waitForDeviceInitialisation = true);

    /** Enable some additional (potentially noisy) debug output for the testable
     * mode. Can be useful if tests
     *  of applications seem to hang for no reason in stepApplication. */
    void debugTestableMode() { enableDebugTestableMode = true; }

    /** Lock the testable mode mutex for the current thread. Internally, a
     * thread-local std::unique_lock<std::mutex> will be created and re-used in
     * subsequent calls within the same thread to this function and to
     *  testableModeUnlock().
     *
     *  This function should generally not be used in user code. */
    static void testableModeLock(const std::string& name);

    /** Unlock the testable mode mutex for the current thread. See also
     * testableModeLock().
     *
     *  Initially the lock will not be owned by the current thread, so the first
     * call to this function will throw an exception (see
     * std::unique_lock::unlock()), unless testableModeLock() has been called
     * first.
     *
     *  This function should generally not be used in user code. */
    static void testableModeUnlock(const std::string& name);

    /** Test if the testable mode mutex is locked by the current thread.
     *
     *  This function should generally not be used in user code. */
    static bool testableModeTestLock();

    /**
     * Set string holding the name of the current thread or the specified thread ID. This is used e.g. for
     * debugging output of the testable mode.
     */
    void setThreadName(const std::string& name);

    /**
     * Get string holding the name of the current thread or the specified thread ID. This is used e.g. for debugging
     * output of the testable mode. Will return "*UNKNOWN_THREAD*" if the name for the given ID has not yet been set.
     */
    static std::string threadName(const boost::thread::id& threadId = boost::this_thread::get_id());

    /** Register the thread in the application system and give it a name. This
     * should be done for all threads used by the application to help with
     * debugging and to allow profiling. */
    static void registerThread(const std::string& name);

    void debugMakeConnections() { enableDebugMakeConnections = true; };

    ModuleType getModuleType() const override { return ModuleType::ModuleGroup; }

    std::string getQualifiedName() const override { return "/" + _name; }

    std::string getFullDescription() const override { return ""; }

    /** Special exception class which will be thrown if tests with the testable
     * mode are stalled. Normally this exception should never be caught. The only
     * reason for catching it might be a situation where the expected behaviour of
     * an app is to do nothing and one wants to test this. Note that the stall
     * condition only appears after a certain timeout, thus tests relying on this
     * will take time.
     *
     *  This exception must not be based on a generic exception class to prevent
     * catching it unintentionally. */
    class TestsStalled {};

    /** Enable debug output for a given variable. */
    void enableVariableDebugging(const VariableNetworkNode& node) { debugMode_variableList.insert(node.getUniqueId()); }

    /** Enable debug output for lost data. This will print to stdout everytime data is lost in internal queues as it
     *  is counted with the DataLossCounter module. Do not enable in production environments. Do not call after
     *  initialisation phase of application. */
    void enableDebugDataLoss() { debugDataLoss = true; }

    /** Incremenet counter for how many write() operations have overwritten unread data */
    static void incrementDataLossCounter(const std::string& name);

    static size_t getAndResetDataLossCounter();

    /** Convenience function for creating constants. See
     * VariableNetworkNode::makeConstant() for details. */
    template<typename UserType>
    static VariableNetworkNode makeConstant(UserType value, size_t length = 1, bool makeFeeder = true);

    void registerDeviceModule(DeviceModule* deviceModule);
    void unregisterDeviceModule(DeviceModule* deviceModule);

    LifeCycleState getLifeCycleState() const { return lifeCycleState; }

    VersionNumber getStartVersion() const { return _startVersion; }

    /** Detection mechanism for cirular dependencies of initial values in ApplicationModules */
    struct CircularDependencyDetector {
      /// Call before an ApplicationModule waits for an initial value on the given node. Calls with
      /// non-Application-typed nodes are ignored.
      void registerDependencyWait(VariableNetworkNode& node);

      /// Call after an ApplicationModule has received an initial value on the given node. Calls with
      /// non-Application-typed nodes are ignored.
      void unregisterDependencyWait(VariableNetworkNode& node);

      /// Print modules which are currently waiting for initial values.
      void printWaiters();

      /// Stop the thread before ApplicationBase::terminate() is called.
      void terminate();

      ~CircularDependencyDetector();

     protected:
      /// Start detection thread
      void startDetectBlockedModules();

      /// Function executed in thread
      void detectBlockedModules();

      std::mutex _mutex;
      std::map<Module*, Module*> _waitMap;
      std::map<Module*, std::string> _awaitedVariables;
      std::map<EntityOwner*, VariableNetworkNode> _awaitedNodes;
      std::unordered_set<Module*> _modulesWeHaveWarnedAbout;
      std::unordered_set<std::string> _devicesWeHaveWarnedAbout;
      std::unordered_set<NodeType> _otherThingsWeHaveWarnedAbout;
      boost::thread _thread;

      friend class Application;

    } circularDependencyDetector;

   protected:
    friend class Module;
    friend class VariableNetwork;
    friend class VariableNetworkNode;
    friend class VariableNetworkGraphDumpingVisitor;
    friend class VariableNetworkModuleGraphDumpingVisitor;
    friend class XMLGeneratorVisitor;
    friend class ConnectingDeviceModule;
    friend class StatusAggregator;

    template<typename UserType>
    friend class Accessor;

    template<typename UserType>
    friend class ExceptionHandlingDecorator;

    /**
     *  Find PVs which should be constant. The names of these PVs start with "@CONST@" followed by the value. If such
     *  a name is found, a feeding constant variable is created.
     */
    void findConstantNodes();

    /** Finalise connections, i.e. decide still-undecided details mostly for
     * Device and ControlSystem variables */
    void finaliseNetworks();

    /** Check if all connections are valid. Internally called in initialise(). */
    void checkConnections();

    /** Obtain the lock object for the testable mode lock for the current thread.
     * The returned object has thread_local storage duration and must only be used
     * inside the current thread. Initially (i.e. after the first call in one
     * particular thread) the lock will not be owned by the returned object, so it
     * is important to catch the corresponding exception when calling
     * std::unique_lock::unlock(). */
    static std::unique_lock<std::timed_mutex>& getTestableModeLockObject();

    /** Register the connections to constants for previously unconnected nodes. */
    void processUnconnectedNodes();

    /** Make the connections between accessors as requested in the initialise()
     * function. */
    void makeConnections();

    /** Apply optimisations to the VariableNetworks, e.g. by merging networks
     * sharing the same feeder. */
    void optimiseConnections();

    /** Make the connections for a single network */
    void makeConnectionsForNetwork(VariableNetwork& network);

    /** Scan for circular dependencies and mark all affcted consuming nodes.
     *  This can only be done after all connections have been established. */
    void markCircularConsumers(VariableNetwork& variableNetwork);

    /** UserType-dependent part of makeConnectionsForNetwork() */
    template<typename UserType>
    void typedMakeConnection(VariableNetwork& network);

    /** Helper function to set consumer implementations in typedMakeConnection() */
    template<typename UserType>
    std::list<std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>, VariableNetworkNode>>
        setConsumerImplementations(VariableNetworkNode const& feeder, std::list<VariableNetworkNode> consumers);

    /** Functor class to call typedMakeConnection() with the right template
     * argument. */
    struct TypedMakeConnectionCaller {
      TypedMakeConnectionCaller(Application& owner, VariableNetwork& network);

      template<typename PAIR>
      void operator()(PAIR&) const;

      Application& _owner;
      VariableNetwork& _network;
      mutable bool done{false};
    };

    /** Register a connection between two VariableNetworkNode */
    VariableNetwork& connect(VariableNetworkNode a, VariableNetworkNode b);

    /** Perform the actual connection of an accessor to a device register */
    template<typename UserType>
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> createDeviceVariable(VariableNetworkNode const& node);

    /** Create a process variable with the PVManager, which is exported to the
       control system adapter. nElements will be the array size of the created
       variable. */
    template<typename UserType>
    boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> createProcessVariable(VariableNetworkNode const& node);

    /** Create a local process variable which is not exported. The first element
     * in the returned pair will be the sender, the second the receiver. If two
     * nodes are passed, the first node should be the sender and the second the
     * receiver. */
    template<typename UserType>
    std::pair<boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>,
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>>>
        createApplicationVariable(VariableNetworkNode const& node, VariableNetworkNode const& consumer = {});

    /** List of InternalModules */
    std::list<boost::shared_ptr<InternalModule>> internalModuleList;

    /** List of variable networks */
    std::list<VariableNetwork> networkList;

    /** List of constant variable nodes */
    std::list<VariableNetworkNode> constantList;

    /** Map of trigger consumers to their corresponding TriggerFanOuts. Note: the
     * key is the ID (address) of the externalTiggerImpl. */
    std::map<const void*, boost::shared_ptr<TriggerFanOut>> triggerMap;

    /** Map of control system type VariableNetworkNodes handed out by ControlSystemModules. This is used to hand out
     *  the same node again if the same variable is requested another time, to ensure the connections are registered in
     *  the same network. */
    std::map<std::string, VariableNetworkNode> controlSystemVariables;

    /** Create a new, empty network */
    VariableNetwork& createNetwork();

    /** Instance of VariableNetwork to indicate an invalid network */
    VariableNetwork invalidNetwork;

    /** Map of DeviceBackends used by this application. The map key is the alias
     * name from the DMAP file */
    std::map<std::string, boost::shared_ptr<ChimeraTK::DeviceBackend>> deviceMap;

    /** Map of DeviceModules. The alias name is the key.*/
    std::map<std::string, DeviceModule*> deviceModuleMap;

    /** Flag if connections should be made in testable mode (i.e. the
     * TestableModeAccessorDecorator is put around all push-type input accessors
     * etc.). */
    bool testableMode{false};

    /** Flag which is set by the TestFacility in runApplication() at the beginning. This is used to make sure
     *  runApplication() is called by the TestFacility and not manually. */
    bool testFacilityRunApplicationCalled{false};

    /** Flag whether initialise() has been called already, to make sure it doesn't get called twice. */
    bool initialiseCalled{false};

    /** Flag whether run() has been called already, to make sure it doesn't get called twice. */
    bool runCalled{false};

    /** Mutex used in testable mode to take control over the application threads.
     * Use only through the lock object obtained through
     * getLockObjectForCurrentThread().
     *
     *  This member is static, since it should survive destroying an application
     * instance and creating a new one. Otherwise getTestableModeLockObject()
     * would not work, since it relies on thread_local instances which have to be
     * static. The static storage duration presents no problem in either case,
     * since there can only be one single instance of Application at a time (see
     * ApplicationBase constructor). */
    static std::timed_mutex testableMode_mutex;

    /** Semaphore counter used in testable mode to check if application code is
     * finished executing. This value may only be accessed while holding the
     * testableMode_mutex. */
    size_t testableMode_counter{0};

    /** Semaphore counter used in testable mode to check if device initialisation is finished executing. This value may
     *  only be accessed while holding the testableMode_mutex. This counter is a separate counter from
     *  testableMode_counter so stepApplication() can be controlled whether to obey this counter. */
    size_t testableMode_deviceInitialisationCounter{0};

    /** Flag if noisy debug output is enabled for the testable mode */
    bool enableDebugTestableMode{false};

    /** Flag whether to warn about unconnected variables or not */
    bool enableUnconnectedVariablesWarning{false};

    /** Flag if debug output is enabled for creation of the varible connections */
    bool enableDebugMakeConnections{false};

    /** Map from ProcessArray uniqueId to the variable ID for control system
     * variables. This is required for the TestFacility. */
    std::map<size_t, size_t> pvIdMap;

    /** Return a fresh variable ID which can be assigned to a sender/receiver
     * pair. The ID will always be non-zero. */
    static size_t getNextVariableId() {
      static size_t nextId{0};
      return ++nextId;
    }

    /** Last thread which successfully obtained the lock for the testable mode.
     * This is used to prevent spamming repeating messages if the same thread
     * acquires and releases the lock in a loop without another thread
     *  activating in between. */
    boost::thread::id testableMode_lastMutexOwner;

    /** Counter how often the same thread has acquired the testable mode mutex in
     * a row without another thread owning it in between. This is an indicator for
     * the test being stalled due to data send through a process
     *  variable but not read by the receiver. */
    std::atomic<size_t> testableMode_repeatingMutexOwner{false};

    /** Testable mode: like testableMode_counter but broken out for each variable.
     * This is not actually used as a semaphore counter but only in case of a
     * detected stall (see testableMode_repeatingMutexOwner) to print a list of
     * variables which still contain unread values. The index of the map is the
     * unique ID of the variable. */
    std::map<size_t, size_t> testableMode_perVarCounter;

    /** Map of unique IDs to namess, used along with testableMode_perVarCounter to
     * print sensible information. */
    std::map<size_t, std::string> testableMode_names;

    /** Map of unique IDs to process variables which have been decorated with the
     * TestableModeAccessorDecorator. */
    std::map<size_t, boost::shared_ptr<TransferElement>> testableMode_processVars;

    /** Map of unique IDs to flags whether the update mode is UpdateMode::poll (so
     * we do not use the decorator) */
    std::map<size_t, bool> testableMode_isPollMode;

    /** List of variables for which debug output was requested via
     * enableVariableDebugging(). Stored is the unique id of the
     * VariableNetworkNode.*/
    std::unordered_set<const void*> debugMode_variableList;

    /** Counter for how many write() operations have overwritten unread data */
    std::atomic<size_t> dataLossCounter{0};

    /** Flag whether to debug data loss (as counted with the data loss counter). */
    bool debugDataLoss{false};

    /** Life-cycle state of the application */
    std::atomic<LifeCycleState> lifeCycleState{LifeCycleState::initialisation};

    /** Version number used at application start, e.g. to propagate initial values */
    VersionNumber _startVersion;

    /** Map of atomic invalidity counters for each circular dependency network.
     *  The key is the hash of network which serves as a unique identifier.
     *   The invalidity counter is atomic so it can be accessed from all modules in the network.
     */
    std::map<size_t, std::atomic<uint64_t>> circularNetworkInvalidityCounters;

    /** The networks of circlular dependencies, reachable by their hash, which serves as unique ID
     */
    std::map<size_t, std::list<EntityOwner*>> circularDependencyNetworks;

    /** Map of thread names */
    std::map<boost::thread::id, std::string> threadNames;

    std::mutex m_threadNames;

    template<typename UserType>
    friend class
        TestableModeAccessorDecorator; // needs access to the testableMode_mutex and testableMode_counter and the idMap

    friend class TestFacility;  // needs access to testableMode_variables
    friend class DeviceModule;  // needs access to testableMode_variables
    friend class TriggerFanOut; // needs access to testableMode_variables

    friend class ControlSystemModule; // needs access to controlSystemVariables

    template<typename UserType>
    friend class DebugPrintAccessorDecorator; // needs access to the idMap
    template<typename UserType>
    friend class MetaDataPropagatingRegisterDecorator; // needs to access circularNetworkInvalidityCounters
    friend class ApplicationModule;                    // needs to access circularNetworkInvalidityCounters

    VersionNumber getCurrentVersionNumber() const override {
      throw ChimeraTK::logic_error("getCurrentVersionNumber() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    void setCurrentVersionNumber(VersionNumber) override {
      throw ChimeraTK::logic_error("setCurrentVersionNumber() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    DataValidity getDataValidity() const override {
      throw ChimeraTK::logic_error("getDataValidity() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    void incrementDataFaultCounter() override {
      throw ChimeraTK::logic_error("incrementDataFaultCounter() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    void decrementDataFaultCounter() override {
      throw ChimeraTK::logic_error("decrementDataFaultCounter() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    std::list<EntityOwner*> getInputModulesRecursively([[maybe_unused]] std::list<EntityOwner*> startList) override {
      throw ChimeraTK::logic_error("getInputModulesRecursively() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
    size_t getCircularNetworkHash() override {
      throw ChimeraTK::logic_error("getCircularNetworkHash() called on the application. This is probably "
                                   "caused by incorrect ownership of variables/accessors or VariableGroups.");
    }
  };

  /*********************************************************************************************************************/

  template<typename UserType>
  VariableNetworkNode Application::makeConstant(UserType value, size_t length, bool makeFeeder) {
    return VariableNetworkNode::makeConstant(makeFeeder, value, length);
  }

  /*********************************************************************************************************************/

} /* namespace ChimeraTK */
