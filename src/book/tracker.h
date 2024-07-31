/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <cassert>
#include "constants.h"

namespace book {

template <class Order>
struct BaseTracker {
  typedef Order OrderPtr;

  BaseTracker(const Order& order) :
    is_bid_(order->is_bid()),
    price_(order->price()),
    qty_(order->qty()),
    funds_(order->funds()),
    filled_qty_(0),
    filled_cost_(0),
    avg_price_(0),
    order_(order) { }
  
  /* virtual destructor to make BaseTracker polymorphic */
  virtual ~BaseTracker() = default;

  bool is_bid() const { return is_bid_; }
  double price() const { return price_; }

  void fill(double fill_qty, double fill_cost) {
    if(funds_ != 0 && fill_cost + filled_cost_ > funds_) {
      throw new std::runtime_error("Market buy fill exceeds funds");
    }

    if(qty_ != 0 && fill_qty + filled_qty_ > qty_) {
      throw new std::runtime_error("Fill qty exceeds order qty");
    }

    avg_price_ = (avg_price_ * filled_qty_  + fill_cost) / (filled_qty_ + fill_qty); 
    filled_cost_ += fill_cost;
    filled_qty_ += fill_qty;
  }
  
  bool filled() const {
    if(funds_ != 0)
      return (funds_ - filled_cost_) < MIN_ORDER_FUNDS;
    else
      return (qty_ - filled_qty_) < MIN_ORDER_QTY;
  }

  double qty_on_book() const {
    return price_ == 0 ? 0 : qty_ - filled_qty_;
  }

  double open_qty() const {
    assert(qty_ != 0);
    return qty_ - filled_qty_;
  }

  double tradable_qty(double price) const {
    /* limiting factor is qty only */
    if(funds_ == 0)
      return qty_ - filled_qty_;

    /* limiting factor is funds only */
    if(qty_ == 0)
      return floor((funds_ - filled_cost_) / price / TRADE_QTY_INCREMENT) * TRADE_QTY_INCREMENT;

    /* limiting factors are both qty and funds */
    return std::min(qty_ - filled_qty_, floor((funds_ - filled_cost_) / price / TRADE_QTY_INCREMENT) * TRADE_QTY_INCREMENT);
  }

  const Order& ptr() const {
    return order_;
  }

  double filled_qty() const {
    return filled_qty_;
  }

  double filled_cost() const {
    return filled_cost_;
  }

  double avg_price() const {
    return avg_price_;
  }

  void change_open_qty(double delta) {
    assert(qty_ != 0);
    assert(delta >= 0 || -delta <= qty_ - filled_qty_);

    qty_ += delta;
  }

protected:
  const bool is_bid_;
  double price_;
  double qty_;
  const double funds_;
  double filled_qty_;
  double filled_cost_;
  double avg_price_;
  const Order order_;
};


}