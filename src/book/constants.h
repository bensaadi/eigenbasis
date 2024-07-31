/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

#pragma once

#include <unordered_map>

namespace book {

const double EPSILON = 1e-14;
const double MIN_ORDER_QTY = 1e-6;

/* qty at which a market order will be considered filled
 if only this much or less funds is remaining  */
const double MIN_ORDER_FUNDS = 0.01;


/* tracker.tradable_qty() returns an amount that can
  exceed the funds. we round down to its nearest TRADE_QTY_INCREMENT */
const double TRADE_QTY_INCREMENT = 1e-7;

const double TAKER_FEE_RATE = 0.01;
const double MAKER_FEE_RATE = 0.005;

const size_t DEFAULT_DEPTH_SIZE = 30;

}