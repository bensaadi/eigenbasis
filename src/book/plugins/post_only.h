/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <book/plugin.h>
#include <book/exceptions.h>
#include <book/types.h>

namespace book {
namespace plugins {

template <class OrderPtr>
struct PostOnlyTracker {
  PostOnlyTracker(const OrderPtr& order) :
    post_only_(order->post_only()) {}
  
  bool post_only() const { return post_only_; };

  private:
    bool post_only_;
};

struct PostOnlyOrder {
  virtual bool post_only() const = 0;
};

template <class Tracker>
class PostOnlyPlugin :
public Plugin<Tracker>
{
protected:
  typedef typename Tracker::OrderPtr OrderPtr;

  void should_trade(
    Tracker& taker,
    Tracker& maker,
    CancelReasons& taker_reason,
    CancelReasons& maker_reason)
  {
    /* only check taker  */
    if(taker.post_only()) taker_reason = CancelReasons::post_only;
  }
};

}
}