/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <book/types.h>

namespace book {

class InterruptMatchingException : public std::exception {
public:
  InterruptMatchingException(CancelReasons reason) : reason_(reason) {}

  CancelReasons reason() { return reason_; };

private:
  CancelReasons reason_;
};


}