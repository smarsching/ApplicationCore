/*
 * VariableNetwork.cc
 *
 *  Created on: Jun 14, 2016
 *      Author: Martin Hierholzer
 */

#include <libxml++/libxml++.h>

#include "VariableNetwork.h"
#include "Accessor.h"
#include "Application.h"

namespace ChimeraTK {

  /*********************************************************************************************************************/

  bool VariableNetwork::hasAppNode(AccessorBase *a, AccessorBase *b) const {
    if(feeder.getType() == NodeType::Application) {
      if(a == &(feeder.getAppAccessor()) || (b != nullptr && b == &(feeder.getAppAccessor())) ) return true;
    }

    // search for a and b in the inputAccessorList
    size_t c = count_if( consumerList.begin(), consumerList.end(),
        [a,b](const VariableNetworkNode n) {
          if(n.getType() != NodeType::Application) return false;
          return a == &(n.getAppAccessor()) || ( b != nullptr && b == &(n.getAppAccessor()) );
        } );

    if(c > 0) return true;
    return false;
  }

  /*********************************************************************************************************************/

  bool VariableNetwork::hasFeedingNode() const {
    if(feeder.getType() == NodeType::invalid) return false;
    return true;
  }

  /*********************************************************************************************************************/

  size_t VariableNetwork::countConsumingNodes() const {
    return consumerList.size();
  }

  /*********************************************************************************************************************/

  size_t VariableNetwork::countFixedImplementations() const {
    size_t count = 0;
    if(feeder.hasImplementation()) count++;
    count += count_if( consumerList.begin(), consumerList.end(),
        [](const VariableNetworkNode n) {
          return n.hasImplementation();
        } );
    return count;
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addAppNode(AccessorBase &a) {
    if(hasAppNode(&a)) return;  // already in the network

    // create Node structure
    VariableNetworkNode node(a);
    node.setOwner(this);

    // if node is feeding, save as feeder for this network
    if(a.isFeeding()) {
      // make sure we only have one feeding node per network
      if(hasFeedingNode()) {
        throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
            "Trying to add a feeding accessor to a network already having a feeding accessor.");
      }
      // update value type
      valueType = &(a.getValueType());
      // update engineering unit
      engineeringUnit = a.getUnit();
      // update feeder
      feeder = node;
    }
    // not is not feeding, add it to list of consumers
    else {
      // add node to consumer list
      consumerList.push_back(node);
    }
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addNode(VariableNetworkNode &a) {
    if(a.hasOwner()) {  // already in the network
      assert( &(a.getOwner()) == this );    /// @todo TODO merge networks?
      return;
    }

    // create Node structure
    a.setOwner(this);

    // if node is feeding, save as feeder for this network
    if(a.getDirection() == VariableDirection::feeding) {
      // make sure we only have one feeding node per network
      if(hasFeedingNode()) {
        throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
            "Trying to add a feeding accessor to a network already having a feeding accessor.");
      }
      // update value type
      valueType = &(a.getValueType());
      // update engineering unit
      engineeringUnit = a.getUnit();
      // update feeder
      feeder = a;
    }
    // not is not feeding, add it to list of consumers
    else {
      // add node to consumer list
      consumerList.push_back(a);
    }
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addConsumingPublication(const std::string& name) {
    VariableNetworkNode node(name, VariableDirection::consuming);
    node.setOwner(this);
    consumerList.push_back(node);
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addFeedingPublication(AccessorBase &a, const std::string& name) {
    addFeedingPublication(a.getValueType(), a.getUnit(), name);
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addFeedingPublication(const std::type_info &typeInfo, const std::string& unit, const std::string& name) {
    if(hasFeedingNode()) {
      throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
          "Trying to add control-system-to-device publication to a network already having a feeding accessor.");
    }
    feeder = VariableNetworkNode(name, VariableDirection::feeding);
    feeder.setOwner(this);
    valueType = &typeInfo;
    engineeringUnit = unit;
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addConsumingDeviceRegister(const std::string &deviceAlias, const std::string &registerName) {
    VariableNetworkNode node(deviceAlias, registerName, UpdateMode::push, VariableDirection::consuming);
    node.setOwner(this);
    consumerList.push_back(node);
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addFeedingDeviceRegister(AccessorBase &a, const std::string &deviceAlias,
      const std::string &registerName, UpdateMode mode) {
    addFeedingDeviceRegister(a.getValueType(), a.getUnit(), deviceAlias, registerName, mode);
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addFeedingDeviceRegister(const std::type_info &typeInfo, const std::string& unit
      , const std::string &deviceAlias, const std::string &registerName, UpdateMode mode) {
    if(hasFeedingNode()) {
      throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
          "Trying to add a feeding device register to a network already having a feeding accessor.");
    }
    feeder = VariableNetworkNode(deviceAlias, registerName, mode, VariableDirection::feeding);
    feeder.setOwner(this);
    valueType = &typeInfo;
    engineeringUnit = unit;
  }

  /*********************************************************************************************************************/

  void VariableNetwork::dump(const std::string& linePrefix) const {
    std::cout << linePrefix << "VariableNetwork {" << std::endl;
    std::cout << linePrefix << "  value type = " << valueType->name() << ", engineering unit = " << engineeringUnit << std::endl;
    std::cout << linePrefix << "  trigger type = ";
    try {
      TriggerType tt = getTriggerType();
      if(tt == TriggerType::feeder) std::cout << "feeder" << std::endl;
      if(tt == TriggerType::pollingConsumer) std::cout << "pollingConsumer" << std::endl;
      if(tt == TriggerType::external) std::cout << "external" << std::endl;
      if(tt == TriggerType::none) std::cout << "none" << std::endl;
    }
    catch(ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork> &e) {
      std::cout << "**error**" << std::endl;
    }
    std::cout << linePrefix << "  feeder";
    feeder.dump();
    std::cout << linePrefix << "  consumers: " << consumerList.size() << std::endl;
    size_t count = 0;
    for(auto &consumer : consumerList) {
      std::cout << linePrefix << "    # " << ++count << ":";
      consumer.dump();
    }
    if(hasExternalTrigger) {
      std::cout << linePrefix << "  external trigger network:" << std::endl;;
      assert(externalTrigger != nullptr);
      externalTrigger->dump("    ");
    }
    std::cout << linePrefix << "}" << std::endl;
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addTriggerReceiver(VariableNetwork *network) {
    VariableNetworkNode node(network);
    node.setOwner(this);
    consumerList.push_back(node);
  }

  /*********************************************************************************************************************/

  void VariableNetwork::addTrigger(AccessorBase &trigger) {

    if(hasExternalTrigger) {
      throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
          "Only one external trigger per variable network is allowed.");
    }

    // find the network the triggering accessor is connected to
    VariableNetwork &otherNetwork = Application::getInstance().findOrCreateNetwork(&trigger);
    otherNetwork.addAppNode(trigger);

    // add the found network as trigger
    addTrigger(otherNetwork);

  }

  /*********************************************************************************************************************/

  void VariableNetwork::addTrigger(VariableNetwork &trigger) {

    if(hasExternalTrigger) {
      throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
          "Only one external trigger per variable network is allowed.");
    }

    // add ourselves as a trigger receiver to the other network
    trigger.addTriggerReceiver(this);

    // set flag and store pointer to other network
    hasExternalTrigger = true;
    externalTrigger = &trigger;

  }

  /*********************************************************************************************************************/

  VariableNetwork::TriggerType VariableNetwork::getTriggerType() const {
    // network has an external trigger
    if(hasExternalTrigger) {
      if(feeder.getMode() == UpdateMode::push) {
        throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
            "Providing an external trigger to a variable network which is fed by a pushing variable is not allowed.");
      }
      return TriggerType::external;
    }
    // network is fed by a pushing node
    if(feeder.getMode() == UpdateMode::push) {
      return TriggerType::feeder;
    }
    // network is fed by a poll-type node: must have exactly one polling consumer
    size_t nPollingConsumers = count_if( consumerList.begin(), consumerList.end(),
        [](const VariableNetworkNode n) { return n.getMode() == UpdateMode::poll; } );
    if(nPollingConsumers != 1) {
      throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
          "In a network with a poll-type feeder and no external trigger, there must be exactly one polling consumer.");
    }
    return TriggerType::pollingConsumer;
  }

  /*********************************************************************************************************************/

  void VariableNetwork::check() {
    // must have consuming nodes
    if(countConsumingNodes() == 0) {
      throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
          "Illegal variable network found: no consuming nodes connected!");
    }

    // must have a feeding node
    if(!hasFeedingNode()) {
      throw ApplicationExceptionWithID<ApplicationExceptionID::illegalVariableNetwork>(
          "Illegal variable network found: no feeding node connected!");
    }

    // all nodes must have this network as the owner
    assert(&(feeder.getOwner()) == this);
    for(auto &consumer : consumerList) {
      assert(&(consumer.getOwner()) == this);
    }

    // if the feeder is an application node, it must be in push mode
    if(feeder.getType() == NodeType::Application) {
      assert(feeder.getMode() == UpdateMode::push);
    }

    // check if trigger is correctly defined (the return type doesn't matter, only the checks done in the function are needed)
    getTriggerType();
  }

  /*********************************************************************************************************************/

  VariableNetwork& VariableNetwork::getExternalTrigger() {
    if(getTriggerType() != TriggerType::external) {
      throw ApplicationExceptionWithID<ApplicationExceptionID::illegalParameter>(
          "VariableNetwork::getExternalTrigger() may only be called if the trigger type is external.");
    }
    return *externalTrigger;
  }

}
