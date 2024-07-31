/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

namespace book {

class BookPrice {
public:
  BookPrice(bool is_bid, double price) : is_bid_(is_bid), price_(price) {}

  bool matches(double rhs) const {
    if(price_ == rhs)
      return true;
    if(is_bid_)
      return rhs < price_ || price_ == 0;
    return price_ < rhs || rhs == 0;
  }

  bool operator <(double rhs) const {
    if(price_ == 0)
      return rhs != 0;
    else if(rhs == 0)
      return false;
    else if(is_bid_)
      return rhs < price_ ;
    else
      return price_ < rhs;
  }

  bool operator ==(double rhs) const {
    return price_ == rhs;
  }

  bool operator !=(double rhs) const {
    return !(price_ == rhs);
  }

  bool operator > (double rhs) const {
    return price_!= 0 && ((rhs == 0) || (is_bid_ ? (rhs > price_) : (price_ > rhs)));
  }

  bool operator <=(double rhs) const {
    return *this < rhs || *this == rhs;
  }

  bool operator >=(double rhs) const {
    return *this > rhs || *this == rhs;
  }

  bool operator <(const BookPrice & rhs) const {
    return *this < rhs.price_;
  }

  bool operator ==(const BookPrice & rhs) const {
    return *this == rhs.price_;
  }

  bool operator !=(const BookPrice & rhs) const {
    return *this != rhs.price_;
  }

  bool operator >(const BookPrice & rhs) const {
    return *this > rhs.price_;
  }

  double price() const {
    return price_;
  }

  bool is_bid() const {
    return is_bid_;
  }

  bool is_market() const {
    return price_ == 0;
  }

private:
  bool is_bid_;
  double price_;
};

inline bool operator < (double price, const BookPrice & key) {
  return key > price;
}

inline bool operator > (double price, const BookPrice & key) {
  return key < price;
}

inline bool operator == (double price, const BookPrice & key) {
  return key == price;
}

inline bool operator != (double price, const BookPrice & key) {
  return key != price;
}

inline bool operator <= (double price, const BookPrice & key) {
  return key >= price;
}

inline bool operator >= (double price, const BookPrice & key) {
  return key <= price;
}

}
