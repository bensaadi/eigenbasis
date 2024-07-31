#include "uint128.h"

namespace utils {


const uint128 uint128_0(0, 0);
const uint128 uint128_1(0, 1);

uint128 operator<<(const uint32_t & lhs, const uint128 & rhs){
    return uint128(0, lhs) << rhs;
}

uint128 operator<<(const uint64_t & lhs, const uint128 & rhs){
    return uint128(0, lhs) << rhs;
}



uint128 & uint128::operator+=(const uint128 & rhs){
    hi += rhs.hi + ((lo + rhs.lo) < lo);
    lo += rhs.lo;
    return *this;
}

uint128 uint128::operator-(const uint128 & rhs) const{
    return uint128(hi - rhs.hi - ((lo - rhs.lo) > lo), lo - rhs.lo);
}


uint128& uint128::operator++(){
  return *this += uint128_1;
}


uint128 & uint128::operator<<=(const uint128 & rhs){
    *this = *this << rhs;
    return *this;
}


uint128 uint128::operator<<(const uint128 & rhs) const{
    const uint64_t shift = rhs.lo;
    if (((bool) rhs.hi) || (shift >= 128)){
        return uint128_0;
    }
    else if (shift == 64){
        return uint128(lo, 0);
    }
    else if (shift == 0){
        return *this;
    }
    else if (shift < 64){
        return uint128((hi << shift) + (lo >> (64 - shift)), lo << shift);
    }
    else if ((128 > shift) && (shift > 64)){
        return uint128(lo << (shift - 64), 0);
    }
    else{
        return uint128_0;
    }
}

uint128 uint128::operator>>(const uint128 & rhs) const{
    const uint64_t shift = rhs.lo;
    if (((bool) rhs.hi) || (shift >= 128)){
        return uint128_0;
    }
    else if (shift == 64){
        return uint128(0, hi);
    }
    else if (shift == 0){
        return *this;
    }
    else if (shift < 64){
        return uint128(hi >> shift, (hi << (64 - shift)) + (lo >> shift));
    }
    else if ((128 > shift) && (shift > 64)){
        return uint128(0, (hi >> (shift - 64)));
    }
    else{
        return uint128_0;
    }
}
uint128 uint128::operator>>(const uint32_t & rhs) const{
    const uint64_t shift = rhs;
    if ((shift >= 128)){
        return uint128_0;
    }
    else if (shift == 64){
        return uint128(0, hi);
    }
    else if (shift == 0){
        return *this;
    }
    else if (shift < 64){
        return uint128(hi >> shift, (hi << (64 - shift)) + (lo >> shift));
    }
    else if ((128 > shift) && (shift > 64)){
        return uint128(0, (hi >> (shift - 64)));
    }
    else{
        return uint128_0;
    }
}


uint8_t uint128::bits() const{
    uint8_t out = 0;
    if (hi){
        out = 64;
        uint64_t up = hi;
        while (up){
            up >>= 1;
            out++;
        }
    }
    else{
        uint64_t low = lo;
        while (low){
            low >>= 1;
            out++;
        }
    }
    return out;
}


std::pair <uint128, uint128> uint128::divmod(const uint128 & lhs, const uint128 & rhs) const{
  if (rhs == uint128_0){
    throw std::domain_error("Error: division or modulus by 0");
  }
  else if (rhs == uint128_1){
    return std::pair <uint128, uint128> (lhs, uint128_0);
  }
  else if (lhs == rhs){
    return std::pair <uint128, uint128> (uint128_1, uint128_0);
  }
  else if ((lhs == uint128_0) || (lhs < rhs)){
    return std::pair <uint128, uint128> (uint128_0, lhs);
  }

  std::pair <uint128, uint128> qr (uint128_0, uint128_0);
  for(uint8_t x = lhs.bits(); x > 0; x--){
    qr.first  <<= uint128_1;
    qr.second <<= uint128_1;

    if((lhs >> (uint32_t)(x - 1U)) & (uint32_t)1){
      ++qr.second;
    }

    if(qr.second >= rhs){
      qr.second -= rhs;
      ++qr.first;
    }
  }
  return qr;
}


uint128 uint128::operator&(const uint128 & rhs) const{
  return uint128(hi & rhs.hi, lo & rhs.lo);
}

uint128 uint128::operator&(const uint32_t & rhs) const{
  return uint128(0, lo & rhs);
}

std::string uint128::to_string(uint8_t base, const unsigned int & len) const {
  if ((base < 2) || (base > 16)){
    throw std::invalid_argument("Base must be in the range [2, 16]");
  }
  std::string out = "";
  if (!(*this)){
      out = "0";
  }
  else{
      std::pair <uint128, uint128> qr(*this, uint128_0);
      int i = 0;
      do{
          qr = divmod(qr.first, uint128(0, base));
          out = "0123456789abcdef"[(uint8_t) qr.second] + out;

          ++i;
          if(i == 12 || i == 16 || i == 20 || i == 24) out = "-" + out; 
      } while (qr.first);
  }
  if (out.size() < len){
      out = std::string(len - out.size(), '0') + out;
  }
  return out;
}


std::ostream & operator<<(std::ostream & stream, const uint128 & rhs){
    if (stream.flags() & stream.oct){
        stream << rhs.to_string(8);
    }
    else if (stream.flags() & stream.dec){
        stream << rhs.to_string(10);
    }
    else if (stream.flags() & stream.hex){
        stream << rhs.to_string(16);
    }
    return stream;
}





}