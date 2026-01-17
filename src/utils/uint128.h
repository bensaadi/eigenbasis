#pragma once

#include <cstdint>
#include <iostream>

namespace utils {


typedef struct uint128 {
  uint64_t hi;
  uint64_t lo;

  uint128() : hi(0), lo(0) {}
  uint128(uint64_t hi_, uint64_t lo_) : hi(hi_), lo(lo_) {}

  std::pair <uint128, uint128> divmod(const uint128 & lhs, const uint128 & rhs) const;

  std::string to_string(uint8_t base = 16, const unsigned int & len = 32) const;


  bool operator==(const uint128& rhs) const {
    return ((hi == rhs.hi) && (lo == rhs.lo));
  }

  bool operator!=(const uint128& rhs) const {
    return ((hi != rhs.hi) | (lo != rhs.lo));
  }

  bool operator<(const uint128 & rhs) const{
      if (hi == rhs.hi){
          return (lo < rhs.lo);
      }
      return (hi < rhs.hi);
  }

  bool operator>=(const uint128 & rhs) const{
      return ((*this > rhs) | (*this == rhs));
  }


  uint128 operator<<(const uint128 & rhs) const;
  friend std::ostream & operator<<(std::ostream & stream, const uint128 & rhs);


  operator uint8_t() const{
      return (uint8_t) lo;
  }

  operator uint32_t() const{
      return (uint32_t) lo;
  }


  uint128 operator>>(const uint128 & rhs) const;
  uint128 operator>>(const uint32_t & rhs) const;

  uint128& operator-=(const uint128 & rhs){
      *this = *this - rhs;
      return *this;
  }

  uint128& operator++();
  uint128 & operator+=(const uint128 & rhs);
  uint128 operator-(const uint128 & rhs) const;
  uint128 & operator<<=(const uint128 & rhs);



  uint128 operator&(const uint128 & rhs) const;
  uint128 operator&(const uint32_t & rhs) const;

  uint128 & operator>>=(const uint128 & rhs){
      *this = *this >> rhs;
      return *this;
  }

  bool operator>(const uint128 & rhs) const{
    if (hi == rhs.hi){
        return (lo > rhs.lo);
    }
    return (hi > rhs.hi);
  }

  uint8_t bits() const;


  operator bool() const{
      return (bool) (hi | lo);
  }

} uint128;


}