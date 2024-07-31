/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <vector>
#include <map>
#include <book/callback.h>
#include <book/tracker.h>
#include <book/book_price.h>

namespace book {

template <class Tracker>
class Plugin {
protected:
  using OrderPtr = typename Tracker::OrderPtr;
  typedef std::multimap<BookPrice, Tracker> TrackerMap;
  typedef Callback<OrderPtr> TypedCallback;

  virtual std::vector<TypedCallback>& callbacks() = 0;
  virtual void emit_callback(const TypedCallback& callback) = 0;
  virtual void emit_cancel_callback(
    const Tracker& tracker, CancelReasons reason) = 0;

  virtual void cancel(const OrderPtr& order, CancelReasons reason) = 0;
  virtual void do_cancel(const OrderPtr& order, CancelReasons reason) = 0;
  virtual void do_replace(const OrderPtr& order, double delta) = 0;
  virtual bool add_tracker(Tracker& taker) = 0;
  virtual bool add(const OrderPtr& order) = 0;

  virtual void process_callbacks() = 0;
  virtual uint32_t symbol_id() const = 0;

  virtual const TrackerMap& bids() const = 0;
  virtual const TrackerMap& asks() const = 0;

  virtual void should_add(
    const Tracker& taker,
    InsertRejectReasons& reason) { }

  virtual void after_add_tracker(
    const Tracker& taker) {}


  virtual void should_trade(
    Tracker& taker,
    Tracker& maker,
    CancelReasons& taker_reason,
    CancelReasons& maker_reason) { }

  virtual void after_trade(
    Tracker& taker,
    Tracker& maker,
    bool maker_is_bid,
    double qty,
    double price) {}
};

}