#pragma once

#include <unistd.h>

#include "depth_constants.h"
#include "depth_level.h"
#include <stdexcept>
#include <map>
#include <cmath>
#include <string.h>
#include <iostream>
#include <functional>
#include <sstream>

namespace depth {

template <int SIZE=30>
class Depth {
public:
  Depth();

  const DepthLevel* bids() const;
  const DepthLevel* last_bid() const;
  const DepthLevel* asks() const;
  const DepthLevel* last_ask() const;
  const DepthLevel* end() const;

  
  DepthLevel* bids();
  DepthLevel* last_bid();
  DepthLevel* asks();  
  DepthLevel* last_ask();

  void add_order(Price price, Quantity qty, bool is_bid);

  void skip_fill(Quantity qty, bool is_bid);

  void fill_order(Price price, 
                  Quantity fill_qty, 
                  bool filled,
                  bool is_bid);

  bool close_order(Price price, Quantity open_qty, bool is_bid);

  void change_qty_order(Price price, double qty_delta, bool is_bid);
  
  bool replace_order(Price current_price,
                     Price new_price,
                     Quantity current_qty_on_book,
                     Quantity effective_delta,
                     bool is_bid);
  
  bool changed() const;
  
  ChangeId last_change() const;
  ChangeId last_published_change() const;
  void published();

private:
  DepthLevel levels_[SIZE*2];
  ChangeId last_change_;
  ChangeId last_published_change_;
  Quantity skip_bid_fill_;
  Quantity skip_ask_fill_;

  typedef std::map<Price, DepthLevel, std::greater<Price>> BidLevelMap;
  typedef std::map<Price, DepthLevel, std::less<Price>> AskLevelMap;
  BidLevelMap hidden_bid_levels_;
  AskLevelMap hidden_ask_levels_;
  
  DepthLevel* find_level(Price price, bool is_bid, bool should_create = true);
  
  void insert_before(DepthLevel* level,
                           bool is_bid,
                           Price price);  
  
  void erase_level(DepthLevel* level, bool is_bid);
};


template <int SIZE> 
Depth<SIZE>::Depth()
: last_change_(0),
  last_published_change_(0),
  skip_bid_fill_(0),
  skip_ask_fill_(0)
{
  memset(levels_, 0, sizeof(DepthLevel) * SIZE * 2);
}

template <int SIZE> 
inline const DepthLevel* 
Depth<SIZE>::bids() const
{
  return levels_;
}

template <int SIZE> 
inline const DepthLevel* 
Depth<SIZE>::asks() const
{
  return levels_ + SIZE;
}

template <int SIZE> 
inline const DepthLevel*
Depth<SIZE>::last_bid() const
{
  return levels_ + (SIZE - 1);
}

template <int SIZE> 
inline const DepthLevel*
Depth<SIZE>::last_ask() const
{
  return levels_ + (SIZE * 2 - 1);
}

template <int SIZE> 
inline const DepthLevel* 
Depth<SIZE>::end() const
{
  return levels_ + (SIZE * 2);
}

template <int SIZE> 
inline DepthLevel* 
Depth<SIZE>::bids()
{
  return levels_;
}

template <int SIZE> 
inline DepthLevel* 
Depth<SIZE>::asks()
{
  return levels_ + SIZE;
}

template <int SIZE> 
inline DepthLevel*
Depth<SIZE>::last_bid()
{
  return levels_ + (SIZE - 1);
}

template <int SIZE> 
inline DepthLevel*
Depth<SIZE>::last_ask()
{
  return levels_ + (SIZE * 2 - 1);
}

template <int SIZE> 
inline void
Depth<SIZE>::add_order(Price price, Quantity qty, bool is_bid)
{
  ChangeId last_change_copy = last_change_;
  DepthLevel* level = find_level(price, is_bid);
  if(level) {
    level->add_order(qty);
    if(!level->is_hidden()) {
      last_change_ = last_change_copy + 1;
      level->last_change(last_change_copy + 1);
    }
  }
}

template <int SIZE> 
inline void
Depth<SIZE>::skip_fill(Quantity qty, bool is_bid)
{
  if(is_bid) {
    if(skip_bid_fill_) {
      std::stringstream msg;
      msg << "Unexpected skip_bid_fill_ " << skip_bid_fill_;
      throw std::runtime_error(msg.str());
    }
    skip_bid_fill_ = qty;
  } else {
    if(skip_ask_fill_) {
      std::stringstream msg;
      msg << "Unexpected skip_ask_fill_ " << skip_ask_fill_;
      throw std::runtime_error(msg.str());
    }
    skip_ask_fill_ = qty;
  }
}

template <int SIZE> 
inline void
Depth<SIZE>::fill_order(
  Price price, 
  Quantity fill_qty, 
  bool filled,
  bool is_bid)
{
  if(is_bid && skip_bid_fill_) {
    skip_bid_fill_ -= fill_qty;
    if(skip_bid_fill_ < EPSILON) skip_bid_fill_ = 0;
  } else if((!is_bid) && skip_ask_fill_) {
    skip_ask_fill_ -= fill_qty;
    if(skip_ask_fill_ < EPSILON) skip_ask_fill_ = 0;
  } else if(filled) {
    close_order(price, fill_qty, is_bid);
  } else {
    change_qty_order(price, -fill_qty, is_bid);
  }
}

template <int SIZE> 
inline bool
Depth<SIZE>::close_order(Price price, Quantity open_qty, bool is_bid)
{
  DepthLevel* level = find_level(price, is_bid, false);
  if(level) {
    if(level->close_order(open_qty)) {
      erase_level(level, is_bid);
      return true;
    } else {
      level->last_change(++last_change_);
    }
  }
  
  return false;
}

template <int SIZE> 
inline void
Depth<SIZE>::change_qty_order(Price price, double qty_delta, bool is_bid)
{
  DepthLevel* level = find_level(price, is_bid, false);
  if(level && qty_delta) {

    if(qty_delta > 0) {
      level->increase_qty(Quantity(qty_delta));
    } else {
      level->decrease_qty(Quantity(-qty_delta));
    }
    level->last_change(++last_change_);
  }
}
 
template <int SIZE> 
inline bool
Depth<SIZE>::replace_order(
  Price current_price,
  Price new_price,
  Quantity current_qty_on_book,
  Quantity effective_delta,
  bool is_bid)
{
  bool erased = false;
  if(current_price == new_price) {
    change_qty_order(current_price, effective_delta, is_bid);
  } else {
    add_order(new_price, current_qty_on_book + effective_delta, is_bid);
    erased = close_order(current_price, current_qty_on_book, is_bid);
  }
  return erased;
}

template <int SIZE> 
DepthLevel*
Depth<SIZE>::find_level(Price price, bool is_bid, bool should_create)
{
  DepthLevel* level = is_bid ? bids() : asks();
  const DepthLevel* past_end = is_bid ? asks() : end();
  for ( ; level != past_end; ++level) {
    if(level->price() == price) {
      break;
    } else if(should_create && level->price() == INVALID_PRICE) {
      level->init(price, false);
      break;
    } else if(is_bid && should_create && level->price() < price) {
      insert_before(level, is_bid, price);
      break;
    } else if((!is_bid) && should_create && level->price() > price) {
      insert_before(level, is_bid, price);
      break;
    }
  }

  if(level == past_end) {
    if(is_bid) {
      BidLevelMap::iterator find_result = hidden_bid_levels_.find(price);

      if(find_result != hidden_bid_levels_.end()) {
        level = &find_result->second;
      } else if(should_create) {
        DepthLevel new_level;
        new_level.init(price, true);
        std::pair<BidLevelMap::iterator, bool> insert_result;
        insert_result = hidden_bid_levels_.insert(
            std::make_pair(price, new_level));
        level = &insert_result.first->second;
      }
    } else {
      AskLevelMap::iterator find_result = hidden_ask_levels_.find(price);

      if(find_result != hidden_ask_levels_.end()) {
        level = &find_result->second;
      } else if(should_create) {
        DepthLevel new_level;
        new_level.init(price, true);
        std::pair<AskLevelMap::iterator, bool> insert_result;
        insert_result = hidden_ask_levels_.insert(
            std::make_pair(price, new_level));
        level = &insert_result.first->second;
      }
    }
  }
  return level;
}

template <int SIZE> 
void
Depth<SIZE>::insert_before(DepthLevel* level, bool is_bid, Price price)
{
  DepthLevel* last_side_level = is_bid ? last_bid() : last_ask();

  if(last_side_level->price() != INVALID_PRICE) {
    DepthLevel hidden_level;
    hidden_level.init(0, true);
    hidden_level = *last_side_level;

    if(is_bid) {
      hidden_bid_levels_.insert(
      std::make_pair(last_side_level->price(), hidden_level));
    } else {
      hidden_ask_levels_.insert(
      std::make_pair(last_side_level->price(), hidden_level));
    }
  }

  DepthLevel* current_level = last_side_level - 1;
  ++last_change_;

  while (current_level >= level) {
    *(current_level + 1) = *current_level;
    if(current_level->price() != INVALID_PRICE) {
      (current_level + 1)->last_change(last_change_);
    }
    --current_level;
   }
   level->init(price, false);
}

template <int SIZE> 
void
Depth<SIZE>::erase_level(DepthLevel* level, bool is_bid)
{
  if(level->is_hidden()) {
    if(is_bid) {
      hidden_bid_levels_.erase(level->price());
    } else {
      hidden_ask_levels_.erase(level->price());
    }
  } else {
    DepthLevel* last_side_level = is_bid ? last_bid() : last_ask();
    ++last_change_;
    DepthLevel* current_level = level;

    while (current_level < last_side_level) {
      if((current_level->price() != INVALID_PRICE) ||
          (current_level == level)) {
        *current_level = *(current_level + 1);
        current_level->last_change(last_change_);
      }
      ++current_level;
    }

    if((level == last_side_level) ||
        (last_side_level->price() != INVALID_PRICE)) {
      if(is_bid) {
        BidLevelMap::iterator best_bid = hidden_bid_levels_.begin();
        if(best_bid != hidden_bid_levels_.end()) {
          *last_side_level = best_bid->second;
          hidden_bid_levels_.erase(best_bid);
        } else {
          last_side_level->init(INVALID_PRICE, false);
          last_side_level->last_change(last_change_);
        }
      } else {
        AskLevelMap::iterator best_ask = hidden_ask_levels_.begin();
        if(best_ask != hidden_ask_levels_.end()) {
          *last_side_level = best_ask->second;
          hidden_ask_levels_.erase(best_ask);
        } else {
          last_side_level->init(INVALID_PRICE, false);
          last_side_level->last_change(last_change_);
        }
      }
      last_side_level->last_change(last_change_);
    }
  }
}

template <int SIZE> 
bool
Depth<SIZE>::changed() const
{
  return last_change_ > last_published_change_;
}


template <int SIZE> 
ChangeId
Depth<SIZE>::last_change() const
{
  return last_change_;
}

template <int SIZE> 
ChangeId
Depth<SIZE>::last_published_change() const
{
  return last_published_change_;
}


template <int SIZE> 
void
Depth<SIZE>::published()
{
  last_published_change_ = last_change_;
}

}
