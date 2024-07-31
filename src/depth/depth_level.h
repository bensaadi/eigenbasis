#pragma once

#include "depth_constants.h"
#include <iostream>

namespace depth {
using namespace book;

class DepthLevel {
public:
  DepthLevel();

  DepthLevel& operator=(const DepthLevel& rhs);
  const Price& price() const;
  uint32_t order_count() const;
  Quantity aggregate_qty() const;
  bool is_hidden() const { return is_hidden_; }

  void init(Price price, bool is_hidden);
  void add_order(Quantity qty);

  void increase_qty(Quantity qty);
  void decrease_qty(Quantity qty);

  void set(Price price, 
           Quantity qty,
           uint32_t order_count,
           ChangeId last_change = 0);
  bool close_order(Quantity qty);

  void last_change(ChangeId last_change) { last_change_ = last_change; }
  ChangeId last_change() const { return last_change_; }
  bool changed_since(ChangeId last_published_change) const;

private:
  Price price_;
  uint32_t order_count_;
  Quantity aggregate_qty_;
  bool is_hidden_;
public:
  ChangeId last_change_;
};

inline bool
DepthLevel::changed_since(ChangeId last_published_change) const
{
  return last_change_ > last_published_change;
}

inline
DepthLevel::DepthLevel()
  : price_(INVALID_PRICE),
  order_count_(0),
  aggregate_qty_(0)
{
}

inline
DepthLevel&
DepthLevel::operator=(const DepthLevel& rhs)
{
  price_ = rhs.price_;
  order_count_ = rhs.order_count_;
  aggregate_qty_ = rhs.aggregate_qty_;
  if(rhs.price_ != INVALID_PRICE) {
    last_change_ = rhs.last_change_;
  }

  return *this;
}

inline
const Price&
DepthLevel::price() const
{
  return price_;
}

inline
void
DepthLevel::init(Price price, bool is_hidden)
{
  price_ = price;
  order_count_ = 0;
  aggregate_qty_ = 0;
  is_hidden_ = is_hidden;
}

inline
uint32_t
DepthLevel::order_count() const
{
  return order_count_;
}

inline
Quantity
DepthLevel::aggregate_qty() const
{
  return aggregate_qty_;
}

inline
void
DepthLevel::add_order(Quantity qty)
{
  ++order_count_;
  aggregate_qty_ += qty;
}

inline
bool
DepthLevel::close_order(Quantity qty)
{
  bool empty = false;
  // If this is the last order, reset the level
  if(order_count_ == 0) {
    throw std::runtime_error("DepthLevel::close_order order count too low");
  } else if(order_count_ == 1) {
    order_count_ = 0;
    aggregate_qty_ = 0;
    empty = true;
  } else {
    --order_count_;
    if(aggregate_qty_ >= qty) {
      aggregate_qty_ -= qty;
    } else {
      throw std::runtime_error("DepthLevel::close_order qty too low");
    }
  }
  return empty;
}

inline
void
DepthLevel::set(Price price, 
  Quantity qty,
  uint32_t order_count,
  ChangeId last_change)
{
  price_ = price;
  aggregate_qty_ = qty;
  order_count_ = order_count;
  last_change_ = last_change;
}

inline
void
DepthLevel::increase_qty(Quantity qty)
{
  aggregate_qty_ += qty;
}

inline
void
DepthLevel::decrease_qty(Quantity qty)
{
  aggregate_qty_ -= qty;
}

}

