#pragma once

#include <unordered_map>
#include <cassert>
#include <cmath>
#include <array>

#include "depth.h"
#include "depth_level.h"

#define BBO_PRECISION 0

namespace depth {

template <typename OrderPtr, int SIZE = 30, int PRECISION = 0>
class DepthBook {
public:
  typedef Depth<SIZE> DepthTracker;

  DepthBook(const std::array<double, 4>& aggregation_levels);

  const DepthTracker& get_depth() const { return depth_; };

  void on_accept(
    const OrderPtr& order,
    double qty);

  void on_fill(
    const OrderPtr& order,
    const OrderPtr& matched_order, 
    double fill_qty,
    double price,
    bool taker_filled,
    bool maker_filled);
  
  void on_cancel(
    const OrderPtr& order,
    const double current_qty_on_book);
  
  void on_replace(
    const OrderPtr& order,
    const double current_qty_on_book,
    const double effective_delta,
    const double new_price);

  void on_order_book_change();

  virtual void on_depth_change() = 0;
  virtual void on_bbo_change() = 0;
private:
  double aggregate(bool is_bid, double price);


protected:
  DepthTracker depth_;
private:
  std::array<double, 4> aggregation_levels_;
};



template <class OrderPtr, int SIZE, int PRECISION>
DepthBook<OrderPtr, SIZE, PRECISION>::DepthBook(const std::array<double, 4>& aggregation_levels) :
  aggregation_levels_(aggregation_levels) {

  }


template <class OrderPtr, int SIZE, int PRECISION>
void DepthBook<OrderPtr, SIZE, PRECISION>::on_accept(
  const OrderPtr& order, double qty)
{
  if(order->price() == 0) return;

  if(qty == order->qty()) {
    depth_.ignore_fill_qty(qty, order->is_bid());
  }

  else {
    depth_.add_order(
      aggregate(order->is_bid(), order->price()), 
      order->accepted_qty(), 
      order->is_bid()
    );
  }
}


template <class OrderPtr, int SIZE, int PRECISION>
void DepthBook<OrderPtr, SIZE, PRECISION>::on_fill(
  const OrderPtr& taker,
  const OrderPtr& maker,
  double fill_qty,
  double price,
  bool taker_filled,
  bool maker_filled)
{
  if(maker->price() != 0) {
    depth_.fill_order(
      aggregate(maker->is_bid(), maker->price()), 
      fill_qty,
      maker_filled,
      maker->is_bid());
  }

  if(taker->price() != 0) {
    depth_.fill_order(
      aggregate(taker->is_bid(), taker->price()),
      fill_qty,
      taker_filled,
      taker->is_bid());
  }
}

template <class OrderPtr, int SIZE, int PRECISION>
void DepthBook<OrderPtr, SIZE, PRECISION>::on_cancel(
  const OrderPtr& order,
  const double current_qty_on_book)
{
  if(order->price() == 0) return;

  depth_.close_order(
    aggregate(order->is_bid(), order->price()), 
    current_qty_on_book,
    order->is_bid());
}


template <class OrderPtr, int SIZE, int PRECISION>
void DepthBook<OrderPtr, SIZE, PRECISION>::on_replace(
  const OrderPtr& order,
  const double current_qty_on_book,
  const double effective_delta,
  const double new_price)
{
  double old_price = aggregate(order->is_bid(), order->price());

  depth_.replace_order(  
    old_price,
    old_price,
    current_qty_on_book,
    effective_delta,
    order->is_bid());
}



template <class OrderPtr, int SIZE, int PRECISION>
void DepthBook<OrderPtr, SIZE, PRECISION>::on_order_book_change()
{
  if(depth_.changed()) {
    on_depth_change();

    ChangeId last_change = depth_.last_published_change();

    if(PRECISION == BBO_PRECISION) {
      if((depth_.bids()->changed_since(last_change)) ||
        (depth_.asks()->changed_since(last_change))) {
        on_bbo_change();
      }
    }

    depth_.published();
  }
}


template <class OrderPtr, int SIZE, int PRECISION>
double DepthBook<OrderPtr, SIZE, PRECISION>::aggregate(
  bool is_bid, double price)
{
  double& exp = aggregation_levels_.at(PRECISION);

  double out;

  if(is_bid)
    out = floor(price / exp) * exp;
  else
    out = ceil(price / exp) * exp;

  return out;
}


}