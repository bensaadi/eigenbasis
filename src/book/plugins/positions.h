/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <iostream>
#include <unordered_map>

#include <book/plugin.h>
#include <book/exceptions.h>
#include <book/types.h>
#include <book/callback.h>

#include <book/plugins/trackers/user_id_tracker.h>

namespace book {
namespace plugins {

struct Position {
  Position() : qty(0), base_price(0) {}

  double qty;
  double base_price;
};

template <class OrderPtr>
struct PositionsTracker : public virtual UserIDTracker<OrderPtr> {
  PositionsTracker(const OrderPtr& order) {
    UserIDTracker<OrderPtr>::set_user_id(order->user_id());
  }
};

class PositionsInterface {
public:
  virtual bool get_position(
    uint64_t user_id, Position& position) = 0;

  virtual void on_position_close(uint64_t user_id) { };
};

template <class Tracker>
class PositionsPlugin : public virtual PositionsInterface,
public Plugin<Tracker> {
protected:
  typedef typename Tracker::OrderPtr OrderPtr;
  typedef Callback<OrderPtr> TypedCallback;

  void after_trade(
    Tracker& taker,
    Tracker& maker,
    bool maker_is_bid,
    double qty,
    double price);

  void update_position(
    Position& pos,
    uint64_t user_id,
    bool is_bid,
    double qty,
    double price);

  bool get_position(
    uint64_t user_id, Position& position);

private:
  std::unordered_map<uint64_t, Position> positions_;
};


template <class Tracker>
void PositionsPlugin<Tracker>::after_trade(
  Tracker& taker,
  Tracker& maker,
  bool maker_is_bid,
  double qty,
  double price)
{
  uint64_t taker_user_id = taker.user_id();
  uint64_t maker_user_id = maker.user_id();

  Position& taker_pos = positions_[taker_user_id];
  Position& maker_pos = positions_[maker_user_id];

  update_position(maker_pos, maker_user_id, maker_is_bid, qty, price);
  update_position(taker_pos, taker_user_id, !maker_is_bid, qty, price);
}

template <class Tracker>
void PositionsPlugin<Tracker>::update_position(
  Position& pos,
  uint64_t user_id,
  bool is_bid,
  double qty,
  double price)
{
  double signed_qty = is_bid ? qty : -qty;
  double new_qty = pos.qty + signed_qty;

  /* increasing a position. not crossing 0 */
  if(pos.qty == 0 || (is_bid == (pos.qty > 0))) {
    pos.base_price =
      (pos.base_price * pos.qty + price * signed_qty)
        / (signed_qty + pos.qty);

    if(pos.qty == 0) {
      this->emit_callback(TypedCallback::position_open(
        user_id, new_qty, pos.base_price));
    } else {
      this->emit_callback(TypedCallback::position_update(
        user_id, new_qty, pos.base_price));
    }
  }

  /* reducing a position */
  else {
    /* closing and potentially reversing the position */
    if(new_qty == 0 || ((new_qty > 0) != (pos.qty > 0))) {
      /* TODO: erase position */
      this->emit_callback(TypedCallback::position_close(user_id));
      on_position_close(user_id);

      /* reversing position */
      if(new_qty != 0) {
        pos.base_price = price;

        this->emit_callback(TypedCallback::position_open(
          user_id, new_qty, pos.base_price));
      }
    } else {
      this->emit_callback(TypedCallback::position_update(
        user_id, new_qty, pos.base_price));
    }
  }

  pos.qty = new_qty;
}

template <class Tracker>
bool PositionsPlugin<Tracker>::get_position(
  uint64_t user_id, Position& position)
{
  auto it = positions_.find(user_id);
  
  if(it == positions_.end())
    return false;

  position = it->second;
  return true;
}

}
}