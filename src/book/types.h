/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

namespace book {

enum InsertRejectReasons : uint8_t {
  dont_reject,
  insert_reject_no_reason,
  reduce_only_increase,
  reduce_only_reverse,
  insufficient_funds,
  qty_too_small,
  funds_too_small,
  duplicate_client_order_id
};

enum CancelRejectReasons : uint8_t {
  dont_cancel_reject,
  cancel_reject_not_found
};

enum ReplaceRejectReasons : uint8_t {
  dont_replace_reject,
  replace_reject_not_found,
  replace_reject_no_qty,
  replace_insufficient_funds
};

enum CancelReasons : uint8_t {
  dont_cancel,
  user_cancel,
  temporary_cancel,
  no_liquidity,
  self_trade,
  engine_shutdown,
  replaced_all_qty,
  post_only,
  reduce_only_match,
  reduce_only_close,
  mm_routed,
  routing_failure,  
};


}