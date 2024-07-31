/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <iostream>

namespace book {
namespace plugins {

template <class OrderPtr>
struct UserIDTracker {
  UserIDTracker() : user_id_(0) {}

  virtual void set_user_id(uint64_t user_id) {
    user_id_ = user_id;
  }

  virtual uint64_t user_id() const {
    return user_id_;
  };

  private:
    uint64_t user_id_;
};

}
}