#ifndef CHIMERATK_EXCEPTION_HANDLING_DECORATOR_H
#define CHIMERATK_EXCEPTION_HANDLING_DECORATOR_H
/*
 * ExceptionHandlingDecorator.h
 *
 *  Created on: Mar 18, 2019
 *      Author: Martin Hierholzer
 */

#include <ChimeraTK/NDRegisterAccessorDecorator.h>

#include "Application.h"

namespace ChimeraTK {

  /** Decorator of the NDRegisterAccessor which facilitates tests of the
   * application */
  template<typename UserType>
  class ExceptionHandlingDecorator : public ChimeraTK::NDRegisterAccessorDecorator<UserType> {
   public:
    ExceptionHandlingDecorator(
        boost::shared_ptr<ChimeraTK::NDRegisterAccessor<UserType>> accessor, DeviceModule& devMod)
    : ChimeraTK::NDRegisterAccessorDecorator<UserType>(accessor), deviceModule(devMod) {}

    bool doWriteTransfer(ChimeraTK::VersionNumber versionNumber = {}) override;

    bool doWriteTransferDestructively(ChimeraTK::VersionNumber versionNumber = {}) override;

    void doReadTransfer() override;

    bool doReadTransferNonBlocking() override;

    bool doReadTransferLatest() override;

    TransferFuture doReadTransferAsync() override;

    
    //void doPreRead() override;
    
    DataValidity dataValidity() const override;

    void interrupt() override;

    void setOwner(EntityOwner* owner);
    
   protected:
    DeviceModule& deviceModule;
    DataValidity localValidity{DataValidity::ok};
    bool genericTransfer(std::function<bool(void)> callable, bool updateOwnerValidityFlag = true);
    void setOwnerValidity(DataValidity newValidity);
    EntityOwner* _owner = {nullptr};
  };

  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(ExceptionHandlingDecorator);

} /* namespace ChimeraTK */

#endif // CHIMERATK_EXCEPTION_HANDLING_DECORATOR_H
