#include <doctest/doctest.h>
#include <memory>
#include <cmath>
#include <stack>

#include <utils/symbols.h>
#include <book/tracker.h>
#include <book/plugins/routable.h>
#include <book/plugins/self_trade_policy.h>
#include "fixtures/order.h"
#include "fixtures/me.h"
#include "fixtures/helpers.h"

#define SYMBOL_ID_1 1
#define MM1_ID 1000
#define MM1_EXCHANGE 2
#define MM2_ID 1001
#define MM2_EXCHANGE 3
#define USER_1 1
#define USER_2 2

#define BUY true
#define SELL false

namespace routing_test {

using namespace utils;

typedef fixtures::OrderWithUserID Order;
typedef std::shared_ptr<Order> OrderPtr;

struct Tracker :
  public virtual book::BaseTracker<OrderPtr>,
  public book::plugins::SelfTradePolicyTracker<OrderPtr>,
  public book::plugins::RoutableTracker<OrderPtr>
{
  Tracker(const OrderPtr& order) :
    book::BaseTracker<OrderPtr>(order),
    book::plugins::SelfTradePolicyTracker<OrderPtr>(order),
    book::plugins::RoutableTracker<OrderPtr>(order) {}
};


typedef fixtures::ME<
  Tracker,
  book::plugins::SelfTradePolicyPlugin<Tracker>,
  book::plugins::RoutablePlugin<Tracker>
> Book_;

enum RoutingScenario {
  ROUTING_SUCCESS,
  ROUTING_FAILURE,
  ROUTING_NO_RESPONSE
};

class Book : public Book_ {
public:
  typedef book::plugins::RoutablePlugin<Tracker>::RoutingRequest RoutingRequest;

  Book(uint32_t symbol_id) : Book_(symbol_id), routing_scenario(ROUTING_SUCCESS) {
    register_market_maker(MM1_ID, MM1_EXCHANGE);
    register_market_maker(MM2_ID, MM2_EXCHANGE);
  }

  const RoutingRequest pop_routing_request() {
    RoutingRequest request = routing_requests_.top();
    routing_requests_.pop();
    return request;
  }

  size_t routing_requests_size() const {
    return routing_requests_.size();
  }

  void clear_routing_requests() {
    std::stack<RoutingRequest> routing_requests;
    routing_requests_.swap(routing_requests);
  }

  RoutingScenario routing_scenario;

protected:
  void on_routing_request(const RoutingRequest& request) {
    routing_requests_.push(request);

    if(routing_scenario == ROUTING_SUCCESS)
      on_routing_success(request.request_id);
    else if(routing_scenario == ROUTING_FAILURE)
      on_routing_failure(request.request_id);
  };

private:
  std::stack<RoutingRequest> routing_requests_;
};

TEST_CASE("routable orders") {
  Book book(SYMBOL_ID_1);

  SUBCASE("non-routing orders: market buy against 3 limit sells") {
    const double p1 = 10046.51;
    const double p2 = 10121.89;
    const double p3 = 10939.23;

    const double q1 = 0.3;
    const double q2 = 0.5;
    const double q3 = 0.7;
    const double f = p1 * q1 + p2 * q2 + 0.5 * q3 * p3;

    book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, p1, q1, 0));
    book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, p2, q2, 0));
    book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, p3, q3, 0));

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 3);

    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_2, BUY, 0, 0, f));

    CHECK(cb.size() == 5);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(EQUALS(cb[0].qty, q1+q2+0.5*q3));
    CHECK(EQUALS(cb[0].avg_price, f/(q1+q2+0.5*q3)));
    INFO(f/(q1+q2+0.5*q3));
    INFO(cb[0].avg_price);

    CHECK(cb[1].type == Book::TypedCallback::cb_trade);
    CHECK(cb[1].qty == q1);
    CHECK(cb[1].price == p1);

    CHECK(cb[2].type == Book::TypedCallback::cb_trade);
    CHECK(cb[2].qty == q2);
    CHECK(cb[2].price == p2);

    CHECK(cb[3].type == Book::TypedCallback::cb_trade);
    CHECK(cb[3].qty == (f - p1 * q1 - p2 * q2)/p3);
    CHECK(cb[3].price == p3);

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 1);
  }

  SUBCASE("1 MM order with 1 user order") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1000.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});

    book.start_recording_callbacks();
    book.add_and_get_cbs(order1);
    Book::Callbacks cb = book.get_recorded_callbacks();

    CHECK(cb.size() == 2);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[1].type == Book::TypedCallback::cb_book_update);

    SUBCASE("adding a user order exactly matching the MM order, routing success") {
      book.routing_scenario = ROUTING_SUCCESS;

      /* it should suppress the fill callback */
      auto order2 = std::make_shared<Order>(USER_1, BUY, 1000.00, 1.0, 0.0);
      order2->order_id((uint128){1, 2});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order2);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 4);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      
      /* callback that is internal only, to change the depth  */
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);

      /* callback that is external only, for the user  */
      CHECK(cb[2].type == Book::TypedCallback::cb_trade);
      CHECK(cb[2].scope == Book::TypedCallback::CbScope::external_only);

      CHECK(cb[3].type == Book::TypedCallback::cb_book_update);


      /* check the routing request */

      auto& routing_request = book.pop_routing_request();

      CHECK(routing_request.exchange_id == MM1_EXCHANGE);
      CHECK(routing_request.symbol_id == SYMBOL_ID_1);
      CHECK(routing_request.is_bid == BUY);
      CHECK(routing_request.qty == 1.0);
      CHECK(routing_request.price == 1000.0);
      CHECK(routing_request.cancel_reason == book::dont_cancel);
    }

    SUBCASE("adding a user order exactly matching the MM order, no response") {
      book.routing_scenario = ROUTING_NO_RESPONSE;

      /* it should suppress the fill callback */
      auto order2 = std::make_shared<Order>(USER_1, BUY, 1000.00, 1.0, 0.0);
      auto order3 = std::make_shared<Order>(USER_1, BUY, 1000.00, 1.0, 0.0);
      order2->order_id((uint128){1, 2});
      order3->order_id((uint128){1, 3});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order2);
      book.add_and_get_cbs(order3);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 5);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      
      /* callback that is internal only, to change the depth  */
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);

      CHECK(cb[2].type == Book::TypedCallback::cb_book_update);

      /* order3 should just ignore the busy MM order, and rest on the book*/
      CHECK(cb[3].type == Book::TypedCallback::cb_order_accept);

      CHECK(cb[4].type == Book::TypedCallback::cb_book_update);


      /* check the routing request */

      auto& routing_request = book.pop_routing_request();

      CHECK(routing_request.exchange_id == MM1_EXCHANGE);
      CHECK(routing_request.symbol_id == SYMBOL_ID_1);
      CHECK(routing_request.is_bid == BUY);
      CHECK(routing_request.qty == 1.0);
      CHECK(routing_request.price == 1000.0);
      CHECK(routing_request.cancel_reason == book::dont_cancel);
    }

  }

  SUBCASE("1 MM order with 1 user limit order") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1000.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});

    Book::Callbacks cb = book.add_and_get_cbs(order1);
    
    CHECK(cb.size() == 2);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[1].type == Book::TypedCallback::cb_book_update);

    SUBCASE("adding a user order exactly matching the MM order, routing success") {
      book.routing_scenario = ROUTING_SUCCESS;

      /* it should suppress the fill callback */
      auto order2 = std::make_shared<Order>(USER_1, BUY, 1000.00, 1.5, 0.0);
      order2->order_id((uint128){1, 2});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order2);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 6);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      
      /* callback that is internal only, to change the depth  */
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);

      /* callback that is internal only, to change the depth  */
      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].scope == Book::TypedCallback::CbScope::suppress_callback);


      /* callback that is external only, for the user  */
      CHECK(cb[3].type == Book::TypedCallback::cb_trade);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::external_only);

      CHECK(cb[4].type == Book::TypedCallback::cb_order_accept);

      CHECK(cb[5].type == Book::TypedCallback::cb_book_update);


      /* check the routing request */

      auto& routing_request = book.pop_routing_request();

      CHECK(routing_request.exchange_id == MM1_EXCHANGE);
      CHECK(routing_request.symbol_id == SYMBOL_ID_1);
      CHECK(routing_request.is_bid == BUY);
      CHECK(routing_request.qty == 1.0);
      CHECK(routing_request.price == 1000.0);
      CHECK(routing_request.cancel_reason == book::dont_cancel);
    }

  }

}

TEST_CASE("routable orders") {
  Book book(SYMBOL_ID_1);

  SUBCASE("3 MM order with 1 user order, same exchange") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1232.00, 1.0, 0.0);
    auto order2 = std::make_shared<Order>(MM1_ID, SELL, 1532.00, 1.0, 0.0);
    auto order3 = std::make_shared<Order>(MM1_ID, SELL, 1422.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});
    order2->order_id((uint128){1, 2});
    order3->order_id((uint128){1, 3});

    book.add_and_get_cbs(order1);
    book.add_and_get_cbs(order2);
    book.add_and_get_cbs(order3);
 
    SUBCASE("adding a user order fully matching the MM order, routing success") {
      book.routing_scenario = ROUTING_SUCCESS;

      auto order4 = std::make_shared<Order>(USER_1, BUY, 2000.00, 3.0, 0.0);
      order4->order_id((uint128){1, 4});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order4);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 8);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);

      CHECK(cb[2].type == Book::TypedCallback::cb_trade);
      CHECK(cb[2].scope == Book::TypedCallback::CbScope::internal_only);

      CHECK(cb[3].type == Book::TypedCallback::cb_trade);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::internal_only);

      CHECK(cb[4].type == Book::TypedCallback::cb_trade);
      CHECK(cb[4].scope == Book::TypedCallback::CbScope::external_only);

      CHECK(cb[5].type == Book::TypedCallback::cb_trade);
      CHECK(cb[5].scope == Book::TypedCallback::CbScope::external_only);

      CHECK(cb[6].type == Book::TypedCallback::cb_trade);
      CHECK(cb[6].scope == Book::TypedCallback::CbScope::external_only);

      CHECK(cb[7].type == Book::TypedCallback::cb_book_update);

      auto& routing_request = book.pop_routing_request();

      CHECK(routing_request.exchange_id == MM1_EXCHANGE);
      CHECK(routing_request.symbol_id == SYMBOL_ID_1);
      CHECK(routing_request.is_bid == BUY);
      CHECK(routing_request.qty == 3.0);
      CHECK(routing_request.price == 1532.00);
      CHECK(routing_request.cancel_reason == book::dont_cancel);
    }
  }

}

TEST_CASE("routable orders") {
  Book book(SYMBOL_ID_1);

  SUBCASE("match with MM orders then user order") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1232.00, 1.0, 0.0);
    auto order2 = std::make_shared<Order>(USER_2, SELL, 1532.00, 1.0, 0.0);
    auto order3 = std::make_shared<Order>(MM1_ID, SELL, 1422.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});
    order2->order_id((uint128){1, 2});
    order3->order_id((uint128){1, 3});

    book.add_and_get_cbs(order1);
    book.add_and_get_cbs(order2);
    book.add_and_get_cbs(order3);
 
    SUBCASE("adding a user order fully matching the MM order, routing success") {
      book.routing_scenario = ROUTING_SUCCESS;

      auto order4 = std::make_shared<Order>(USER_1, BUY, 2000.00, 3.0, 0.0);
      order4->order_id((uint128){1, 4});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order4);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 9);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);

      CHECK(cb[2].type == Book::TypedCallback::cb_trade);
      CHECK(cb[2].scope == Book::TypedCallback::CbScope::internal_only);

      CHECK(cb[3].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::suppress_callback);

      CHECK(cb[4].type == Book::TypedCallback::cb_trade);
      CHECK(cb[4].scope == Book::TypedCallback::CbScope::external_only);

      CHECK(cb[5].type == Book::TypedCallback::cb_trade);
      CHECK(cb[5].scope == Book::TypedCallback::CbScope::external_only);

      CHECK(cb[6].type == Book::TypedCallback::cb_trade);
      CHECK(cb[6].scope == Book::TypedCallback::CbScope::broadcast_to_all);

      CHECK(cb[7].type == Book::TypedCallback::cb_order_accept);

      CHECK(cb[8].type == Book::TypedCallback::cb_book_update);

      auto& routing_request = book.pop_routing_request();

      CHECK(routing_request.exchange_id == MM1_EXCHANGE);
      CHECK(routing_request.symbol_id == SYMBOL_ID_1);
      CHECK(routing_request.is_bid == BUY);
      CHECK(routing_request.qty == 2.0);
      CHECK(routing_request.price == 1422.00);
      CHECK(routing_request.cancel_reason == book::dont_cancel);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }
  }

  SUBCASE("match with MM order, then user order, then MM order") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1232.00, 1.0, 0.0);
    auto order2 = std::make_shared<Order>(USER_2, SELL, 1332.00, 1.0, 0.0);
    auto order3 = std::make_shared<Order>(MM1_ID, SELL, 1422.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});
    order2->order_id((uint128){1, 2});
    order3->order_id((uint128){1, 3});

    book.add_and_get_cbs(order1);
    book.add_and_get_cbs(order2);
    book.add_and_get_cbs(order3);
 
    SUBCASE("adding a user order fully matching the MM order, routing success") {
      book.routing_scenario = ROUTING_SUCCESS;

      auto order4 = std::make_shared<Order>(USER_1, BUY, 2000.00, 3.0, 0.0);
      order4->order_id((uint128){1, 4});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order4);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 9);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);

      /* mm trade */
      
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[1].order->order_id() == order4->order_id());
      CHECK(cb[1].maker_order->order_id() == order1->order_id());

      /* then we hit a user order, this cancel is made by should_trade then suppressed */
      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].scope == Book::TypedCallback::CbScope::suppress_callback);
      CHECK(cb[2].order->order_id() == order4->order_id());

      /* routing success for order 1 */
      CHECK(cb[3].type == Book::TypedCallback::cb_trade);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[3].maker_order->order_id() == order1->order_id());

      /* user trade*/
      CHECK(cb[4].type == Book::TypedCallback::cb_trade);
      CHECK(cb[4].scope == Book::TypedCallback::CbScope::broadcast_to_all);
      CHECK(cb[4].maker_order->order_id() == order2->order_id());

      /* routing order 3 */
      CHECK(cb[5].type == Book::TypedCallback::cb_trade);
      CHECK(cb[5].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[5].maker_order->order_id() == order3->order_id());

      /* routing success for order 3 */
      CHECK(cb[6].type == Book::TypedCallback::cb_trade);
      CHECK(cb[6].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[6].maker_order->order_id() == order3->order_id());

      CHECK(cb[7].type == Book::TypedCallback::cb_order_accept);

      CHECK(cb[8].type == Book::TypedCallback::cb_book_update);

      CHECK(book.routing_requests_size() == 2);

      auto& r2 = book.pop_routing_request();
      auto& r1 = book.pop_routing_request();

      CHECK(r1.exchange_id == MM1_EXCHANGE);
      CHECK(r1.symbol_id == SYMBOL_ID_1);
      CHECK(r1.is_bid == BUY);
      CHECK(r1.qty == 1.0);
      CHECK(r1.price == 1232.00);
      CHECK(r1.cancel_reason == book::dont_cancel);

      CHECK(r2.exchange_id == MM1_EXCHANGE);
      CHECK(r2.symbol_id == SYMBOL_ID_1);
      CHECK(r2.is_bid == BUY);
      CHECK(r2.qty == 1.0);
      CHECK(r2.price == 1422.00);
      CHECK(r2.cancel_reason == book::dont_cancel);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }
  }


  SUBCASE("match with MM order, then user order, then MM order, then user order") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1232.00, 1.0, 0.0);
    auto order2 = std::make_shared<Order>(USER_2, SELL, 1332.00, 1.0, 0.0);
    auto order3 = std::make_shared<Order>(MM1_ID, SELL, 1422.00, 1.0, 0.0);
    auto order5 = std::make_shared<Order>(USER_2, SELL, 1522.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});
    order2->order_id((uint128){1, 2});
    order3->order_id((uint128){1, 3});
    order5->order_id((uint128){1, 5});

    book.add_and_get_cbs(order1);
    book.add_and_get_cbs(order2);
    book.add_and_get_cbs(order3);
    book.add_and_get_cbs(order5);
 
    SUBCASE("adding a user order fully matching the MM order, routing success") {
      book.routing_scenario = ROUTING_SUCCESS;

      auto order4 = std::make_shared<Order>(USER_1, BUY, 2000.00, 4.0, 0.0);
      order4->order_id((uint128){1, 4});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order4);
      Book::Callbacks cb = book.get_recorded_callbacks();


      CHECK(cb.size() == 12);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);

      /* routinig order 1 */
      
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[1].order->order_id() == order4->order_id());
      CHECK(cb[1].maker_order->order_id() == order1->order_id());

      /* then we hit a user order, this cancel is made by should_trade then suppressed */
      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].scope == Book::TypedCallback::CbScope::suppress_callback);
      CHECK(cb[2].order->order_id() == order4->order_id());

      /* routing success for order 1 */
      CHECK(cb[3].type == Book::TypedCallback::cb_trade);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[3].maker_order->order_id() == order1->order_id());

      /* user trade 1 */
      CHECK(cb[4].type == Book::TypedCallback::cb_trade);
      CHECK(cb[4].scope == Book::TypedCallback::CbScope::broadcast_to_all);
      CHECK(cb[4].maker_order->order_id() == order2->order_id());

      /* routing order 3 */
      CHECK(cb[5].type == Book::TypedCallback::cb_trade);
      CHECK(cb[5].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[5].maker_order->order_id() == order3->order_id());

      /* then we hit a user order, this cancel is made by should_trade then suppressed */
      CHECK(cb[6].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[6].scope == Book::TypedCallback::CbScope::suppress_callback);
      CHECK(cb[6].order->order_id() == order4->order_id());

      /* routing success for order 3 */
      CHECK(cb[7].type == Book::TypedCallback::cb_trade);
      CHECK(cb[7].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[7].maker_order->order_id() == order3->order_id());

      /* user trade 2*/
      CHECK(cb[8].type == Book::TypedCallback::cb_trade);
      CHECK(cb[8].scope == Book::TypedCallback::CbScope::broadcast_to_all);
      CHECK(cb[8].maker_order->order_id() == order5->order_id());

      CHECK(cb[9].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[10].type == Book::TypedCallback::cb_order_accept);

      CHECK(cb[11].type == Book::TypedCallback::cb_book_update);

      CHECK(book.routing_requests_size() == 2);

      auto& r2 = book.pop_routing_request();
      auto& r1 = book.pop_routing_request();

      CHECK(r1.exchange_id == MM1_EXCHANGE);
      CHECK(r1.symbol_id == SYMBOL_ID_1);
      CHECK(r1.is_bid == BUY);
      CHECK(r1.qty == 1.0);
      CHECK(r1.price == 1232.00);
      CHECK(r1.cancel_reason == book::dont_cancel);

      CHECK(r2.exchange_id == MM1_EXCHANGE);
      CHECK(r2.symbol_id == SYMBOL_ID_1);
      CHECK(r2.is_bid == BUY);
      CHECK(r2.qty == 1.0);
      CHECK(r2.price == 1422.00);
      CHECK(r2.cancel_reason == book::dont_cancel);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }
  }



  SUBCASE("match with MM order, then rest on book") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1000.00, 1.0, 0.0);
    auto order2 = std::make_shared<Order>(MM1_ID, SELL, 2000.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});
    order2->order_id((uint128){1, 2});

    book.add_and_get_cbs(order1);
    book.add_and_get_cbs(order2);
 
    SUBCASE("adding a user order fully matching the MM order, routing success") {
      book.routing_scenario = ROUTING_SUCCESS;

      auto order3 = std::make_shared<Order>(USER_1, BUY, 1000.00, 2.0, 0.0);
      order3->order_id((uint128){1, 3});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order3);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 6);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);

      /* routinig order 1 */
      
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[1].order->order_id() == order3->order_id());
      CHECK(cb[1].maker_order->order_id() == order1->order_id());

      /* then we hit a user order, this cancel is made by should_trade then suppressed */
      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].order->order_id() == order3->order_id());

      /* routing success for order 1 */
      CHECK(cb[3].type == Book::TypedCallback::cb_trade);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[3].maker_order->order_id() == order1->order_id());

      CHECK(cb[4].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[5].type == Book::TypedCallback::cb_book_update);

      CHECK(book.routing_requests_size() == 1);
      auto& r1 = book.pop_routing_request();

      CHECK(r1.exchange_id == MM1_EXCHANGE);
      CHECK(r1.symbol_id == SYMBOL_ID_1);
      CHECK(r1.is_bid == BUY);
      CHECK(r1.qty == 1.0);
      CHECK(r1.price == 1000.00);
      CHECK(r1.cancel_reason == book::dont_cancel);

      CHECK(book.bids().size() == 1);
      CHECK(book.asks().size() == 1);
    }


    SUBCASE("adding a user order fully matching the MM order, routing failure") {
      book.routing_scenario = ROUTING_FAILURE;

      auto order3 = std::make_shared<Order>(USER_1, BUY, 1000.00, 2.0, 0.0);
      order3->order_id((uint128){1, 3});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order3);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 5);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);

      /* routinig order 1 */
      
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[1].order->order_id() == order3->order_id());
      CHECK(cb[1].maker_order->order_id() == order1->order_id());

      /* then we hit a user order, this cancel is made by should_trade then suppressed */
      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].scope == Book::TypedCallback::CbScope::suppress_callback);
      CHECK(cb[2].order->order_id() == order3->order_id());

      /* order3 should be cancelled */
      CHECK(cb[3].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[3].reason == book::CancelReasons::routing_failure);
      CHECK(cb[3].order->order_id() == order3->order_id());

      CHECK(cb[4].type == Book::TypedCallback::cb_book_update);

      CHECK(book.routing_requests_size() == 1);
      auto& r1 = book.pop_routing_request();

      CHECK(r1.exchange_id == MM1_EXCHANGE);
      CHECK(r1.symbol_id == SYMBOL_ID_1);
      CHECK(r1.is_bid == BUY);
      CHECK(r1.qty == 1.0);
      CHECK(r1.price == 1000.00);
      CHECK(r1.cancel_reason == book::dont_cancel);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 1);
    }
  }



  SUBCASE("match with MM orders in 2 different exchanges") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1000.00, 1.0, 0.0);
    auto order2 = std::make_shared<Order>(MM2_ID, SELL, 2000.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});
    order2->order_id((uint128){1, 2});

    book.add_and_get_cbs(order1);
    book.add_and_get_cbs(order2);
 
    SUBCASE("adding a user order fully matching the two MM orders, routing success") {
      book.routing_scenario = ROUTING_SUCCESS;

      auto order3 = std::make_shared<Order>(USER_1, BUY, 2000.00, 2.0, 0.0);
      order3->order_id((uint128){1, 3});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order3);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 8);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);

      /* routinig order 1 */
      
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[1].order->order_id() == order3->order_id());
      CHECK(cb[1].maker_order->order_id() == order1->order_id());

      /* then we hit a different exchange mm order,
        this cancel is made by should_trade then suppressed */
      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].order->order_id() == order3->order_id());

      /* routing success for order 1 */
      CHECK(cb[3].type == Book::TypedCallback::cb_trade);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[3].maker_order->order_id() == order1->order_id());

      /* now we match with mm2 order*/
      CHECK(cb[4].type == Book::TypedCallback::cb_trade);
      CHECK(cb[4].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[4].maker_order->order_id() == order2->order_id());

      /* routing success for order 2 */
      CHECK(cb[5].type == Book::TypedCallback::cb_trade);
      CHECK(cb[5].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[5].maker_order->order_id() == order2->order_id());

      CHECK(cb[6].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[7].type == Book::TypedCallback::cb_book_update);

      CHECK(book.routing_requests_size() == 2);
      auto& r2 = book.pop_routing_request();
      auto& r1 = book.pop_routing_request();

      CHECK(r1.exchange_id == MM1_EXCHANGE);
      CHECK(r1.symbol_id == SYMBOL_ID_1);
      CHECK(r1.is_bid == BUY);
      CHECK(r1.qty == 1.0);
      CHECK(r1.price == 1000.00);
      CHECK(r1.cancel_reason == book::dont_cancel);

      CHECK(r2.exchange_id == MM2_EXCHANGE);
      CHECK(r2.symbol_id == SYMBOL_ID_1);
      CHECK(r2.is_bid == BUY);
      CHECK(r2.qty == 1.0);
      CHECK(r2.price == 2000.00);
      CHECK(r2.cancel_reason == book::dont_cancel);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }

    SUBCASE("adding a user order fully matching the two MM orders, routing failure") {
      book.routing_scenario = ROUTING_FAILURE;

      auto order3 = std::make_shared<Order>(USER_1, BUY, 2000.00, 2.0, 0.0);
      order3->order_id((uint128){1, 3});
      book.start_recording_callbacks();
      book.add_and_get_cbs(order3);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 5);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);

      /* routinig order 1 */
      
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[1].order->order_id() == order3->order_id());
      CHECK(cb[1].maker_order->order_id() == order1->order_id());

      /* then we hit a different exchange mm order,
        this cancel is made by should_trade then suppressed */
      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].order->order_id() == order3->order_id());

      /* routing failure for order 1 */
      CHECK(cb[3].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::external_only);

      /* stop here. never reaching order 2 */
      CHECK(cb[4].type == Book::TypedCallback::cb_book_update);

      
      CHECK(book.routing_requests_size() == 1);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 1);
    }
  }

  SUBCASE("match with MM order, then stp cancel taker with own") {
    auto order1 = std::make_shared<Order>(MM1_ID, SELL, 1000.00, 1.0, 0.0);
    auto order2 = std::make_shared<Order>(USER_1, SELL, 2000.00, 1.0, 0.0);
    order1->order_id((uint128){1, 1});
    order2->stp(book::plugins::SelfTradePolicy::stp_cancel_taker);
    order2->order_id((uint128){1, 2});

    book.add_and_get_cbs(order1);
    book.add_and_get_cbs(order2);
 
    SUBCASE("adding a user order fully matching the MM order, then cancelling") {
      book.routing_scenario = ROUTING_SUCCESS;

      auto order3 = std::make_shared<Order>(USER_1, BUY, 2000.00, 2.0, 0.0);
      order3->stp(book::plugins::SelfTradePolicy::stp_cancel_taker);
      order3->order_id((uint128){1, 3});

      book.start_recording_callbacks();
      book.add_and_get_cbs(order3);
      Book::Callbacks cb = book.get_recorded_callbacks();

      CHECK(cb.size() == 7);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);

      /* routinig order 1 */
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[1].scope == Book::TypedCallback::CbScope::internal_only);
      CHECK(cb[1].order->order_id() == order3->order_id());
      CHECK(cb[1].maker_order->order_id() == order1->order_id());


      /* then we hit a user order, this cancel is made by should_trade then suppressed */
      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].scope == Book::TypedCallback::CbScope::suppress_callback);
      CHECK(cb[2].order->order_id() == order3->order_id());

      /* routing success for order 1 */
      CHECK(cb[3].type == Book::TypedCallback::cb_trade);
      CHECK(cb[3].scope == Book::TypedCallback::CbScope::external_only);
      CHECK(cb[3].maker_order->order_id() == order1->order_id());


      /* cancelled due to stp */
      CHECK(cb[4].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[4].reason == book::CancelReasons::self_trade);
      CHECK(cb[4].scope == Book::TypedCallback::CbScope::broadcast_to_all);
      
      
      CHECK(cb[5].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[6].type == Book::TypedCallback::cb_book_update);

      CHECK(book.routing_requests_size() == 1);
      auto& r1 = book.pop_routing_request();

      CHECK(r1.exchange_id == MM1_EXCHANGE);
      CHECK(r1.symbol_id == SYMBOL_ID_1);
      CHECK(r1.is_bid == BUY);
      CHECK(r1.qty == 1.0);
      CHECK(r1.price == 1000.00);
      CHECK(r1.cancel_reason == book::dont_cancel);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 1);
    }
  }
}

}