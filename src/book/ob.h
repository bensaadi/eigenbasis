/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <map>
#include <vector>
#include <cassert>
#include <stdint.h>
#include <memory>
#include <iostream>

#include "types.h"
#include "book_price.h"
#include "tracker.h"
#include "callback.h"

#define INVOKE_PLUGIN_HOOKS(FN) \
  (void) std::initializer_list<int>{ (Plugins::FN, 0)... };

namespace book {

template <class Tracker, class... Plugins>
class OB : public Plugins... {

public:
  typedef typename Tracker::OrderPtr OrderPtr;
  typedef Callback<OrderPtr> TypedCallback;
  typedef std::vector<TypedCallback> Callbacks;
  typedef std::multimap<BookPrice, Tracker> TrackerMap;

  OB(uint32_t symbol_id);

  bool add(const OrderPtr& order);
  bool add_tracker(Tracker& taker);

  void cancel(const OrderPtr& order, CancelReasons reason);
  void replace(const OrderPtr& order, double delta);
  void set_market_price(double price);

  uint32_t symbol_id() const { return symbol_id_; }
  double market_price() const { return market_price_; }

  const TrackerMap& bids() const { return bids_; }
  const TrackerMap& asks() const { return asks_; }

  /* for callbacks to be accessed from plugins */
  Callbacks& callbacks() { return callbacks_; };

protected:
  bool match(
    Tracker& taker,
    TrackerMap& makers);

  double trade(
    Tracker& taker,
    Tracker& maker);

  bool find(
    const OrderPtr& order,
    typename TrackerMap::iterator& it);

  void emit_callback(const TypedCallback& callback);
  void emit_cancel_callback(
    const Tracker& tracker, CancelReasons reason);

  void process_callbacks();

  void do_cancel(const OrderPtr& order, CancelReasons reason);
  void do_replace(const OrderPtr& order, double delta);
  void replace_to_qty(const OrderPtr& order, double new_open_qty);

  virtual void on_callbacks(const Callbacks& callbacks) = 0;

private:
  uint32_t symbol_id_;
  double market_price_;
  TrackerMap bids_;
  TrackerMap asks_;
  Callbacks callbacks_;
  bool is_taker_cancelled_;
};


template <class Tracker, class... Plugins>
OB<Tracker, Plugins...>::OB(uint32_t symbol_id) :
  symbol_id_(symbol_id),
  market_price_(0),
  is_taker_cancelled_(false)
{
  callbacks_.reserve(20);
}

template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::set_market_price(double price) {
  market_price_ = price;
}

template <class Tracker, class... Plugins>
bool OB<Tracker, Plugins...>::add(const OrderPtr& order) {
  assert(order->qty() != 0 || order->funds() != 0);

  Tracker taker(order);

  InsertRejectReasons reject_reason = dont_reject;
  INVOKE_PLUGIN_HOOKS(should_add(taker, reject_reason));

  if(reject_reason != dont_reject) {
    emit_callback(TypedCallback::reject(order, reject_reason));
    process_callbacks();
    return false;
  }

  /* making accept cb always come before fill cbs */
  size_t accept_cb_index = callbacks_.size();
  emit_callback(TypedCallback::accept(order));
  
  bool matched = add_tracker(taker);

  callbacks_[accept_cb_index].qty = taker.filled_qty();
  callbacks_[accept_cb_index].avg_price = taker.avg_price();

  emit_callback(TypedCallback::book_update());
  process_callbacks();

  return matched;
}


template <class Tracker, class... Plugins>
bool OB<Tracker, Plugins...>::add_tracker(Tracker& taker) {
  bool matched = false;

  TrackerMap& takers = taker.is_bid() ? bids_ : asks_;
  TrackerMap& makers = taker.is_bid() ? asks_ : bids_;

  matched = match(taker, makers);

  if(!taker.filled() && !is_taker_cancelled_) {
    if(taker.price() == 0) {
      emit_callback(TypedCallback::cancel(
        taker.ptr(), 0, taker.filled_qty(), taker.avg_price(), no_liquidity));
    
      INVOKE_PLUGIN_HOOKS(after_add_tracker(taker))  
    }

    else {
      auto it = takers.emplace(std::make_pair(
        BookPrice(taker.is_bid(), taker.price()), std::move(taker)));

      INVOKE_PLUGIN_HOOKS(after_add_tracker(it->second))
    }
  } else {
    INVOKE_PLUGIN_HOOKS(after_add_tracker(taker))  
  }

  is_taker_cancelled_ = false;

  return matched;
}

/**
 * \brief creates trades given on taker and a sorted map of makers
 * \param taker the taker order
 * \param makers sorted map of makers, can be bids_ or asks_
 */

template <class Tracker, class... Plugins>
bool OB<Tracker, Plugins...>::match(
  Tracker& taker,
  TrackerMap& makers)
{
  bool matched = false;
  auto pos = makers.begin(); 
  
  while(pos != makers.end() && !taker.filled()) {
    auto entry = pos++;

    const BookPrice& maker_book_price = entry->first;
    if(!maker_book_price.matches(taker.price())) break;

    Tracker& maker = entry->second;

    CancelReasons taker_reason = dont_cancel,
                  maker_reason = dont_cancel;
    
    INVOKE_PLUGIN_HOOKS(should_trade(
      taker, maker, taker_reason, maker_reason))

    if(maker_reason != dont_cancel) {
      emit_cancel_callback(maker, maker_reason);
      makers.erase(entry);
    }

    if(taker_reason != dont_cancel) {
      emit_cancel_callback(taker, taker_reason);
      is_taker_cancelled_ = true; 
      break;
    }

    if(maker_reason != dont_cancel)
      continue;
    
    double traded = trade(taker, maker);

    if(traded > 0) {
      matched = true;

      if(maker.filled())
        makers.erase(entry);
    }
  }

  return matched;
}


/**
 * \brief attempts to generate a trade given two opposite
 *  orders. generates fill callbacks & updates trackers accordingly 
*/

template <class Tracker, class... Plugins>
double OB<Tracker, Plugins...>::trade(
  Tracker& taker,
  Tracker& maker)
{
  double xprice = maker.price();
  assert(xprice > 0);

  const double taker_qty = taker.tradable_qty(xprice);
  const double maker_qty = maker.tradable_qty(xprice);

  const double fill_qty = std::min(taker_qty, maker_qty);
  const double fill_cost = fill_qty * xprice;

  if(fill_qty > 0) {
    taker.fill(fill_qty, fill_cost);
    maker.fill(fill_qty, fill_cost);

    typename TypedCallback::FillFlags fill_flags = 
      TypedCallback::neither_filled;

    if(taker.filled()) {
      fill_flags = (typename TypedCallback::FillFlags)(
        fill_flags | TypedCallback::taker_filled);
    }

    if(maker.filled()) {
      fill_flags = (typename TypedCallback::FillFlags)(
        fill_flags | TypedCallback::maker_filled);
    }

    emit_callback(TypedCallback::fill(
      taker.ptr(), maker.ptr(), fill_qty, xprice,
      taker.avg_price(), maker.avg_price(), taker.filled_qty(), maker.filled_qty(), fill_flags));

    set_market_price(xprice);

    INVOKE_PLUGIN_HOOKS(after_trade(
      taker, maker, maker.is_bid(), fill_qty, xprice));
  }

  return fill_qty;
}


template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::process_callbacks() {
  on_callbacks(callbacks_);
  callbacks_.clear();
}

template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::emit_callback(const TypedCallback& callback)
{
  callbacks_.push_back(callback);
}


template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::emit_cancel_callback(
  const Tracker& tracker, CancelReasons reason)
{
  callbacks_.push_back(TypedCallback::cancel(
    tracker.ptr(),
    tracker.qty_on_book(),
    tracker.filled_qty(),
    tracker.avg_price(),
    reason));
}


template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::cancel(
  const OrderPtr& order, CancelReasons reason)
{
  do_cancel(order, reason);
  emit_callback(TypedCallback::book_update());
  process_callbacks();
}

template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::do_cancel(
  const OrderPtr& order, CancelReasons reason)
{
  typename TrackerMap::iterator it;

  if(find(order, it)) {
    TrackerMap& trackers = order->is_bid() ? bids_ : asks_;
    Tracker& tracker = it->second;

    /* since cancel() can be called inside match() by plugins,
     * order may be attempted to be cancelled although
     * it is already fully filled. do not emit an extra callback 
     * in this case. */
    if(tracker.filled()) return;

    emit_cancel_callback(tracker, reason);
    trackers.erase(it);
  }

  else if(reason == user_cancel) {
    emit_callback(TypedCallback::cancel_reject(
      order, 0, 0, cancel_reject_not_found));
  }
}


template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::replace(
  const OrderPtr& order, double delta)
{
  do_replace(order, delta);
  process_callbacks();
}

template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::do_replace(
  const OrderPtr& order, double delta)
{
  typename TrackerMap::iterator it;

  if(!find(order, it))
    return emit_callback(TypedCallback::replace_reject(
      order, 0, 0, replace_reject_not_found));

  Tracker& tracker = it->second;

  double open_qty = tracker.qty_on_book();

  if(open_qty == 0)
    return emit_callback(TypedCallback::replace_reject(
      order, tracker.filled_qty(), tracker.avg_price(), replace_reject_no_qty));

  if(delta < 0 && -delta > open_qty) {
    delta = -open_qty;
  }

  tracker.change_open_qty(delta);

  emit_callback(TypedCallback::replace(
    tracker.ptr(), delta, open_qty, tracker.filled_qty(), tracker.avg_price()));

  if(tracker.qty_on_book() < MIN_ORDER_QTY) {
    TrackerMap& trackers = tracker.is_bid() ? bids_ : asks_;
    emit_cancel_callback(tracker, replaced_all_qty);
    trackers.erase(it);
  }

  emit_callback(TypedCallback::book_update());
}

template <class Tracker, class... Plugins>
void OB<Tracker, Plugins...>::replace_to_qty(
  const OrderPtr& order, double new_open_qty)
{
  typename TrackerMap::iterator it;

  if(!find(order, it))
    return emit_callback(TypedCallback::replace_reject(
      order, 0, 0, replace_reject_not_found));

  Tracker& tracker = it->second;

  double open_qty = tracker.qty_on_book();

  if(open_qty == 0)
    return emit_callback(TypedCallback::replace_reject(
      order, tracker.filled_qty(), tracker.avg_price(), replace_reject_no_qty));

  double delta = new_open_qty - open_qty;

  tracker.change_open_qty(delta);

  emit_callback(TypedCallback::replace(
    tracker.ptr(), delta, open_qty, tracker.filled_qty(), tracker.avg_price()));

  if(tracker.qty_on_book() < MIN_ORDER_QTY) {
    TrackerMap& trackers = tracker.is_bid() ? bids_ : asks_;
    emit_cancel_callback(tracker, replaced_all_qty);
    trackers.erase(it);
  }

  emit_callback(TypedCallback::book_update());
  process_callbacks();
}


template <class Tracker, class... Plugins>
bool OB<Tracker, Plugins...>::find(
  const OrderPtr& order,
  typename TrackerMap::iterator& it)
{
  const BookPrice key(order->is_bid(), order->price());
  TrackerMap& trackers = order->is_bid() ? bids_ : asks_;

  for(it = trackers.find(key); it != trackers.end(); ++it) {
    if(it->second.ptr() == order) return true;
    
    else if(key < it->first) {
      it = trackers.end();
      return false;
    }
  }
  
  return false;
}

}
