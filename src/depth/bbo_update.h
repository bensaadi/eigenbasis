#pragma once

namespace depth {

typedef struct BBOUpdate {
  uint32_t symbol_id;
  double bid_qty;
  double bid_price;
  double ask_qty;
  double ask_price;
  double market_price;
} BBOUpdate;
  
}