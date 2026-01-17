#pragma once
#include <book/order.h>
#include <book/plugins/self_trade_policy.h>
#include <book/plugins/post_only.h>
#include <book/plugins/reduce_only.h>
#include <book/plugins/stop_orders.h>

#include <utils/uint128.h>

namespace fixtures {
using namespace book::plugins;
using namespace utils;

class OrderWithUserID : public book::Order, public SelfTradePolicyOrder {
public:
  OrderWithUserID(
    uint32_t user_id,
    bool is_bid,
    double price,
    double qty,
    double funds) :
    user_id_(user_id), is_bid_(is_bid), price_(price),
    qty_(qty), funds_(funds), stp_(stp_cancel_taker) { }

  uint128 order_id() const { return order_id_; };
  void order_id(const uint128& order_id) { order_id_ = order_id; };

  uint32_t user_id() const { return user_id_; };
  bool is_bid() const { return is_bid_; };
  double qty() const { return qty_; };
  double price() const { return price_; };
  double funds() const { return funds_; };
  SelfTradePolicy stp() const { return stp_; };
  void stp(book::plugins::SelfTradePolicy stp) { stp_ = stp; };

protected:
  uint128 order_id_;
  uint32_t user_id_;
  bool is_bid_;
  double price_;
  double qty_;
  double funds_;
  SelfTradePolicy stp_;
};

class OrderWithPostOnly : public OrderWithUserID, public PostOnlyOrder {
public:
  OrderWithPostOnly(
    uint32_t user_id,
    bool is_bid,
    double price,
    double qty,
    double funds,
    bool post_only = false) :
      OrderWithUserID(user_id, is_bid, price, qty, funds),
       post_only_(post_only) { }

  bool post_only() const {
    return post_only_;
  }

private:
  bool post_only_;
};

class OrderWithReduceOnly : public OrderWithUserID, public ReduceOnlyOrder {
public:
  OrderWithReduceOnly(
    uint32_t user_id,
    bool is_bid,
    double price,
    double qty,
    double funds,
    bool reduce_only = false) :
      OrderWithUserID(user_id, is_bid, price, qty, funds),
       reduce_only_(reduce_only) { }

  bool reduce_only() const {
    return reduce_only_;
  }

private:
  bool reduce_only_;
};


class OrderWithStopPrice : public OrderWithUserID, public StopOrder {
public:
  OrderWithStopPrice(
    uint32_t user_id,
    bool is_bid,
    double price,
    double qty,
    double funds,
    double stop_price = 0) :
      OrderWithUserID(user_id, is_bid, price, qty, funds),
       stop_price_(stop_price) { }

  double stop_price() const {
    return stop_price_;
  }

private:
  double stop_price_;
};


}