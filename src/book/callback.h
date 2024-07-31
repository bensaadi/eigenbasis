/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <iostream>

#include "types.h"

namespace book {

static const std::vector<std::string> cbTypeStr = {
  "UNKNOWN",
  "ACCEPT",
  "REJECT",
  "CANCEL",
  "CANCEL REJECT",
  "REPLACE",
  "REPLACE_REJECT",
  "FILL",
  "BOOK UPDATE",
  "TRADE",
  "POSITION OPEN",
  "POSITION UPADTE",
  "POSITION CLOSE"
};

static const std::vector<std::string> cbScopeStr = {
  "SUPPRESS CALLBACK",
  "INTERNAL ONLY",
  "EXTERNAL ONLY",
  "BROADCAST TO ALL",
};

template <typename OrderPtr, typename... Plugins> class OB;

template <typename OrderPtr>
class Callback {
public:
  enum CbType : uint8_t {
    cb_unknown,
    cb_order_accept,
    cb_order_reject,
    cb_order_cancel,
    cb_order_cancel_reject,
    cb_order_replace,
    cb_order_replace_reject,
    cb_order_fill, /* not to be used for calculations. use cb_trade instead */
    cb_book_update,
    cb_trade,
    cb_position_open,
    cb_position_update,
    cb_position_close
  };


  enum FillFlags : uint8_t {
    neither_filled         = 0,
    taker_filled           = 1,
    maker_filled           = 2,
    both_filled            = 3
  };

  /* used for routing */
  enum CbScope : uint8_t {
    suppress_callback      = 0,
    internal_only          = 1,
    external_only          = 2,
    broadcast_to_all       = 3
  };

  Callback();

  std::string to_string() const {
    switch(type) {
      case cb_trade:
        return "[TRADE] " + std::to_string(qty) + " @ " + std::to_string(price);
      case cb_order_cancel:
        return "[CANCEL] reason "+ std::to_string((int)reason) + " order " + order->order_id().to_string() + " ["+cbScopeStr[scope] +"]";
      default:
        return "["+cbTypeStr[type]+"] ["+cbScopeStr[scope] +"]";
    }
  }

  inline friend std::ostream& operator<<(std::ostream & os, const CbType& type) {
    os << cbTypeStr[type];
    return os;
  }

  inline friend std::ostream& operator<<(std::ostream & os, const CbScope& type) {
    os << cbScopeStr[type];
    return os;
  }

  static Callback<OrderPtr> accept(
    const OrderPtr& order);

  static Callback<OrderPtr> reject(
    const OrderPtr& order,
    InsertRejectReasons reason);

  static Callback<OrderPtr> fill(
    const OrderPtr& taker,
    const OrderPtr& maker,
    double fill_qty,
    double price,
    double taker_avg_price,
    double maker_avg_price,
    double taker_total_fill_qty,
    double maker_total_fill_qty,
    uint8_t fill_flags);

  static Callback<OrderPtr> cancel(
    const OrderPtr& order,
    double current_qty_on_book, /* used for depth */
    double filled_qty,
    double avg_price,
    CancelReasons reason);

  static Callback<OrderPtr> replace(
    const OrderPtr& order,
    double effective_delta,
    double current_qty_on_book,
    double filled_qty,
    double avg_price);

  static Callback<OrderPtr> replace_reject(
    const OrderPtr& order,
    double filled_qty,
    double avg_price,
    ReplaceRejectReasons reason);

  static Callback<OrderPtr> cancel_reject(
    const OrderPtr& order,
    double filled_qty,
    double avg_price,
    CancelRejectReasons reason);

  static Callback<OrderPtr> book_update();

  static Callback<OrderPtr> position_open(
    uint32_t user_id,
    double qty,
    double base_price);

  static Callback<OrderPtr> position_close(
    uint32_t user_id);

  static Callback<OrderPtr> position_update(
    uint32_t user_id,
    double qty,
    double base_price);

  CbType type;
  uint8_t flags;
  uint8_t reason;
  OrderPtr order;
  OrderPtr maker_order;
  double qty;
  double price;
  double avg_price;
  double generic_1;
  double generic_2;
  double generic_3;
  uint64_t user_id;
  CbScope scope;
};

template <class OrderPtr>
Callback<OrderPtr>::Callback()
: type(cb_unknown),
  flags(0),
  reason(0),
  order(nullptr),
  maker_order(nullptr),
  qty(0),
  price(0),
  avg_price(0),
  generic_1(0),
  generic_2(0),
  generic_3(0),
  user_id(0),
  scope(broadcast_to_all) { }

template <class OrderPtr>
Callback<OrderPtr> Callback<OrderPtr>::accept(
  const OrderPtr& order)
{
  Callback<OrderPtr> cb;
  cb.type = cb_order_accept;
  cb.order = order;
  /* qty and avg_price updated in this cb if matched */
  return cb;
}

template <class OrderPtr>
Callback<OrderPtr> Callback<OrderPtr>::reject(
  const OrderPtr& order,
  InsertRejectReasons reason)
{
  Callback<OrderPtr> cb;
  cb.type = cb_order_reject;
  cb.order = order;
  cb.qty = 0;
  cb.avg_price = 0;
  cb.reason = reason;
  return cb;
}

template <class OrderPtr>
Callback<OrderPtr> Callback<OrderPtr>::fill(
  const OrderPtr& taker,
  const OrderPtr& maker,
  double fill_qty,
  double price,
  double taker_avg_price,
  double maker_avg_price,
  double taker_total_fill_qty,
  double maker_total_fill_qty,
  uint8_t fill_flags)
{
  Callback<OrderPtr> cb;
  cb.type = cb_trade;
  cb.order = taker;
  cb.maker_order = maker;
  cb.qty = fill_qty;
  cb.price = price;
  cb.avg_price = taker_avg_price;
  cb.generic_1 = maker_avg_price;
  cb.generic_2 = taker_total_fill_qty;
  cb.generic_3 = maker_total_fill_qty;
  cb.flags = fill_flags;
  return cb;
}

template <class OrderPtr>
Callback<OrderPtr> Callback<OrderPtr>::cancel(
  const OrderPtr& order,
  double current_qty_on_book,
  double filled_qty,
  double avg_price,
  CancelReasons reason)
{
  Callback<OrderPtr> cb;
  cb.type = cb_order_cancel;
  cb.order = order;
  cb.qty = filled_qty;
  cb.avg_price = avg_price;
  cb.generic_1 = current_qty_on_book;
  cb.reason = reason;
  return cb;
}

template <class OrderPtr>
Callback<OrderPtr> Callback<OrderPtr>::replace(
  const OrderPtr& order,
  double effective_delta,
  double current_qty_on_book,
  double filled_qty,
  double avg_price)
{
  Callback<OrderPtr> cb;
  cb.type = cb_order_replace;
  cb.order = order;
  cb.generic_1 = effective_delta;
  cb.generic_2 = current_qty_on_book;
  cb.qty = filled_qty;
  cb.avg_price = avg_price;
  return cb;
}


template <class OrderPtr>
Callback<OrderPtr> Callback<OrderPtr>::cancel_reject(
  const OrderPtr& order,
  double filled_qty,
  double avg_price,
  CancelRejectReasons reason)
{
  Callback<OrderPtr> cb;
  cb.type = cb_order_cancel_reject;
  cb.order = order;
  cb.qty = filled_qty;
  cb.avg_price = avg_price;
  cb.reason = reason;
  return cb;
}

template <class OrderPtr>
Callback<OrderPtr> Callback<OrderPtr>::replace_reject(
  const OrderPtr& order,
  double filled_qty,
  double avg_price,
  ReplaceRejectReasons reason)
{
  Callback<OrderPtr> cb;
  cb.type = cb_order_replace_reject;
  cb.order = order;
  cb.qty = filled_qty;
  cb.avg_price = avg_price;
  cb.reason = reason;
  return cb;
}

template <class OrderPtr>
Callback<OrderPtr>
Callback<OrderPtr>::book_update()
{
  Callback<OrderPtr> cb;
  cb.type = cb_book_update;
  return cb;
}

template <class OrderPtr>
Callback<OrderPtr>
Callback<OrderPtr>::position_open(
  uint32_t user_id,
  double qty,
  double base_price)
{
  Callback<OrderPtr> cb;
  cb.type = cb_position_open;
  cb.user_id = user_id;
  cb.qty = qty;
  cb.avg_price = base_price;
  return cb;
}

template <class OrderPtr>
Callback<OrderPtr>
Callback<OrderPtr>::position_close(
  uint32_t user_id)
{
  Callback<OrderPtr> cb;
  cb.type = cb_position_close;
  cb.user_id = user_id;
  return cb;
}


template <class OrderPtr>
Callback<OrderPtr>
Callback<OrderPtr>::position_update(
  uint32_t user_id,
  double qty,
  double base_price)
{
  Callback<OrderPtr> cb;
  cb.type = cb_position_update;
  cb.user_id = user_id;
  cb.qty = qty;
  cb.avg_price = base_price;
  return cb;
}



}