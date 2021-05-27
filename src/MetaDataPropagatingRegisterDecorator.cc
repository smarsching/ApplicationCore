#include "MetaDataPropagatingRegisterDecorator.h"
#include "EntityOwner.h"
#include "VariableNetworkNode.h"
#include "Application.h"
#include <boost/pointer_cast.hpp>

namespace ChimeraTK {

  template<typename T>
  void MetaDataPropagatingRegisterDecorator<T>::doPostRead(TransferType type, bool hasNewData) {
    NDRegisterAccessorDecorator<T, T>::doPostRead(type, hasNewData);

    // update the version number
    if(_target->getAccessModeFlags().has(AccessMode::wait_for_new_data) && type == TransferType::read) {
      _owner->setCurrentVersionNumber(this->getVersionNumber());
    }

    // Check if the data validity flag changed. If yes, propagate this information to the owning module and the application
    if(_dataValidity != lastValidity) {
      if(_dataValidity == DataValidity::faulty) { // data validity changes to faulty
        _owner->incrementDataFaultCounter();
        // external inpput in a circular dependency network
        std::cout << "changed validity for " << _target->getName() << " in circular network "
                  << _owner->getCircularNetworkHash() << std::endl;
        if(_owner->getCircularNetworkHash() && !_isCircularInput) {
          std::cout << "This is " << _target->getName() << " increasing invalidity counter" << std::endl;
          ++(Application::getInstance().circularNetworkInvalidityCounters[_owner->getCircularNetworkHash()]);
        }
      }
      else { // data validity changed to OK
        _owner->decrementDataFaultCounter();
        // external inpput in a circular dependency network
        if(_owner->getCircularNetworkHash() && !_isCircularInput) {
          std::cout << "This is " << _target->getName() << " decreasing invalidity counter" << std::endl;
          --(Application::getInstance().circularNetworkInvalidityCounters[_owner->getCircularNetworkHash()]);
        }
      }
      lastValidity = _dataValidity;
    }
  }

  template<typename T>
  void MetaDataPropagatingRegisterDecorator<T>::doPreWrite(TransferType type, VersionNumber versionNumber) {
    // We cannot use NDRegisterAccessorDecorator<T> here because we need a different implementation of setting the target data validity.
    // So we have a complete implemetation here.
    if(_dataValidity == DataValidity::faulty) { // the application has manualy set the validity to faulty
      _target->setDataValidity(DataValidity::faulty);
    }
    else { // automatic propagation of the owner validity
      _target->setDataValidity(_owner->getDataValidity());
    }

    for(unsigned int i = 0; i < _target->getNumberOfChannels(); ++i) {
      buffer_2D[i].swap(_target->accessChannel(i));
    }
    _target->preWrite(type, versionNumber);
  }

} // namespace ChimeraTK

INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(ChimeraTK::MetaDataPropagatingRegisterDecorator);
