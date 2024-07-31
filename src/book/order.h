/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <book/types.h>

namespace book {

class Order {
public:
  virtual bool is_bid() const = 0;
  virtual double qty() const = 0;
  virtual double price() const = 0;
  virtual double funds() const = 0;
};

}