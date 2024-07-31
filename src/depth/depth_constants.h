#pragma once

#include <book/types.h>
#include <float.h>

namespace depth {

namespace {

typedef double Quantity;
typedef double Price;
typedef uint64_t ChangeId;

const Price INVALID_PRICE(0);
const Price MARKET_ORDER_BID_SORT_PRICE(DBL_MAX);
const Price MARKET_ORDER_ASK_SORT_PRICE(0);

const double EPSILON = 1e-14;

}

}
