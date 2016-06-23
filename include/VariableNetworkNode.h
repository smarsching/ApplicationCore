/*
 * VariableNetworkNode.h
 *
 *  Created on: Jun 23, 2016
 *      Author: Martin Hierholzer
 */

#ifndef CHIMERATK_VARIABLE_NETWORK_NODE_H
#define CHIMERATK_VARIABLE_NETWORK_NODE_H

#include <assert.h>

#include "Flags.h"

namespace xmlpp {
  class Element;
}

namespace ChimeraTK {

  class VariableNetwork;
  class AccessorBase;

  /** Class describing a node of a variable network */
  class VariableNetworkNode {

    public:

      /** Constructor for an Application node */
      VariableNetworkNode(AccessorBase &accessor);

      /** Constructor for a Device node */
      VariableNetworkNode(const std::string &deviceAlias, const std::string &registerName, UpdateMode mode);

      /** Constructor for a ControlSystem node */
      VariableNetworkNode(std::string publicName);

      /** Constructor for a TriggerReceiver node triggering the data transfer of another network */
      VariableNetworkNode(VariableNetwork *networkToTrigger);

      /** Default constructor for an invalid node */
      VariableNetworkNode() {}

      /** Set the owner network of this node. If an owner network is already set, an assertion will be raised */
      void setOwner(VariableNetwork *network);

      /** Function checking if the node requires a fixed implementation */
      bool hasImplementation() const;

      /** Compare two nodes */
      bool operator==(const VariableNetworkNode& other) const;
      bool operator!=(const VariableNetworkNode& other) const;

      /** Print node information to std::cout */
      void dump() const;

      /** Create an XML node describing this network node as seen by the control syste. If the type is not
       *  NodeType::ControlSystem, this function does nothing. Otherwise the correct directory hierarchy will be
       *  created (if not yet existing) and a variable tag will be created containing the externally visible
       *  properties of this variable. */
      void createXML(xmlpp::Element *rootElement) const;

      /** Getter for the properties */
      NodeType getType() const { return type; }
      UpdateMode getMode() const { return mode; }
      VariableNetwork& getOwner() const { assert(network != nullptr); return *network; }
      AccessorBase& getAppAccessor() const { assert(appNode != nullptr); return *appNode; }
      VariableNetwork& getTriggerReceiver() const { assert(triggerReceiver != nullptr); return *triggerReceiver; }
      const std::string& getPublicName() const { assert(type == NodeType::ControlSystem); return publicName; }
      const std::string& getDeviceAlias() const { assert(type == NodeType::Device); return deviceAlias; }
      const std::string& getRegisterName() const { assert(type == NodeType::Device); return registerName; }

    private:

      /** Type of the node (Application, Device, ControlSystem, Trigger) */
      NodeType type{NodeType::invalid};

      /** Update mode: poll or push */
      UpdateMode mode{UpdateMode::invalid};

      /** The network this node belongs to */
      VariableNetwork *network{nullptr};

      /** Pointer to Accessor if type == Application */
      AccessorBase *appNode{nullptr};

      /** Pointer to network which should be triggered by this node */
      VariableNetwork *triggerReceiver{nullptr};

      /** Public name if type == ControlSystem */
      std::string publicName;

      /** Device information if type == Device */
      std::string deviceAlias;
      std::string registerName;
  };

} /* namespace ChimeraTK */

#endif /* CHIMERATK_VARIABLE_NETWORK_NODE_H */
