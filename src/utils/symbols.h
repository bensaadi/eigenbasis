#pragma once

namespace utils {

/* symbol_id is 20-bit
 * quote is 5-bit (max 31) 
 * and base is 15-bit */

constexpr uint32_t MAX_QUOTE_LOG = 5;
constexpr uint32_t MAX_QUOTE_MASK = (1 << MAX_QUOTE_LOG)- 1;

/* base,quote->symbol */
inline uint32_t bqtos(uint32_t base, uint32_t quote) {
  return (base << MAX_QUOTE_LOG) + quote;
}

/* symbol->base */
inline uint32_t stob(uint32_t symbol) {
  return symbol >> MAX_QUOTE_LOG;
}

/* symbol->quote */
inline uint32_t stoq(uint32_t symbol) {
  return symbol & MAX_QUOTE_MASK;
}

}