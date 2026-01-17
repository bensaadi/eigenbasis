#include <doctest/doctest.h>
#include <memory>
#include <cmath>

#include <book/types.h>
#include <book/plugins/self_trade_policy.h>
#include <book/plugins/positions.h>
#include <book/plugins/stop_orders.h>
#include "fixtures/order.h"
#include "fixtures/me.h"
#include "fixtures/helpers.h"

namespace matching_test {

#define SYMBOL_ID_1 1
#define USER_1 1
#define USER_2 2

#define BUY true
#define SELL false

typedef fixtures::OrderWithStopPrice Order;
typedef std::shared_ptr<Order> OrderPtr;


struct Tracker :
  public virtual book::BaseTracker<OrderPtr>
{
  Tracker(const OrderPtr& order) :
    book::BaseTracker<OrderPtr>(order) {}
};


typedef fixtures::ME<
  Tracker,
  book::plugins::StopOrdersPlugin<Tracker>
> Book;


TEST_CASE("adding orders") {

}

}