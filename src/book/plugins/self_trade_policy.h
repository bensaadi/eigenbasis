/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <book/plugin.h>
#include <book/exceptions.h>
#include <book/types.h>
#include <book/plugins/trackers/user_id_tracker.h>

namespace book {
namespace plugins {

enum SelfTradePolicy : uint8_t {
  stp_cancel_taker = 1,
  stp_cancel_maker = 2,
  stp_cancel_both = 3
};

template <class OrderPtr>
struct SelfTradePolicyTracker :
public virtual UserIDTracker<OrderPtr>
{
  SelfTradePolicyTracker(const OrderPtr& order) : stp_(order->stp()) {
    UserIDTracker<OrderPtr>::set_user_id(order->user_id());
  }

  SelfTradePolicy stp() const {
    return stp_;
  }

private:
  SelfTradePolicy stp_;
};


struct SelfTradePolicyOrder {
  virtual uint32_t user_id() const = 0;
  virtual SelfTradePolicy stp() const = 0;
};

template <class Tracker>
class SelfTradePolicyPlugin :
public Plugin<Tracker>
{
protected:
  typedef typename Tracker::OrderPtr OrderPtr;
  typedef Tracker PluginTracker;

  void should_trade(
    Tracker& taker,
    Tracker& maker,
    CancelReasons& taker_reason,
    CancelReasons& maker_reason)
  {
    if(taker.user_id() != maker.user_id()) return;

    SelfTradePolicy combined_stp =
      (SelfTradePolicy)(taker.stp() | maker.stp());

    if(combined_stp & stp_cancel_taker) taker_reason = self_trade;
    if(combined_stp & stp_cancel_maker) maker_reason = self_trade;
  }
};

}
}