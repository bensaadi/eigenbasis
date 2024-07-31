#include <doctest/doctest.h>
#include <memory>
#include <cmath>

#include <book/types.h>
#include <book/plugins/self_trade_policy.h>
#include <book/plugins/positions.h>
#include "fixtures/order.h"
#include "fixtures/me.h"
#include "fixtures/helpers.h"

namespace matching_test {

#define SYMBOL_ID_1 1
#define USER_1 1
#define USER_2 2

#define BUY true
#define SELL false

typedef fixtures::OrderWithUserID Order;
typedef std::shared_ptr<Order> OrderPtr;


struct Tracker :
  public virtual book::BaseTracker<OrderPtr>,
  public book::plugins::SelfTradePolicyTracker<OrderPtr>
{
  Tracker(const OrderPtr& order) :
    book::BaseTracker<OrderPtr>(order),
    book::plugins::SelfTradePolicyTracker<OrderPtr>(order) {}
};


typedef fixtures::ME<
  Tracker,
  book::plugins::SelfTradePolicyPlugin<Tracker>
> Book;


TEST_CASE("adding orders") {
  Book book(SYMBOL_ID_1);

  SUBCASE("adding buy limit order to an empty book") {
    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, 1000.00, 1.0, 0));

    CHECK(cb.size() == 2);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[0].qty == 0);
    CHECK(cb[0].avg_price == 0);

    CHECK(cb[1].type == Book::TypedCallback::cb_book_update);

    CHECK(book.bids().size() == 1);
    CHECK(book.asks().size() == 0);
  }

  SUBCASE("adding sell limit order to an empty book") {
    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, 1000.00, 1.0, 0));

    CHECK(cb.size() == 2);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[0].qty == 0);
    CHECK(cb[0].avg_price == 0);

    CHECK(cb[1].type == Book::TypedCallback::cb_book_update);

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 1);
  }

  SUBCASE("adding buy market order to an empty book") {
    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, 0, 0, 1000.00));

    CHECK(cb.size() == 3);

    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[0].qty == 0);
    CHECK(cb[0].avg_price == 0);

    CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
    CHECK(cb[1].generic_1 == 0);
    CHECK(cb[1].reason == (int)book::CancelReasons::no_liquidity);

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 0);
  }

  SUBCASE("adding sell market order to an empty book") {
    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, 0, 1.0, 0));

    CHECK(cb.size() == 3);

    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[0].qty == 0);
    CHECK(cb[0].avg_price == 0);

    CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
    CHECK(cb[1].generic_1 == 0);
    CHECK(cb[1].reason == (int)book::CancelReasons::no_liquidity);

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 0);
  }

}

TEST_CASE("matching") {
  Book book(SYMBOL_ID_1);


  SUBCASE("market sell against 3 limit buys") {
    const double p1 = 10259.23;
    const double p2 = 10231.89;
    const double p3 = 10216.51;

    const double q1 = 0.3;
    const double q2 = 0.5;
    const double q3 = 0.3;
    const double q = 1.0;

    const double cost = q1 * p1 + q2 * p2 + 0.2 * p3;

    book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, p1, q1, 0));
    book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, p2, q2, 0));
    book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, p3, q3, 0));

    CHECK(book.bids().size() == 3);
    CHECK(book.asks().size() == 0);

    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, 0, q, 0));

    CHECK(cb.size() == 5);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(EQUALS(cb[0].qty, q));
    CHECK(EQUALS(cb[0].avg_price, cost));

    CHECK(cb[1].type == Book::TypedCallback::cb_trade);
    CHECK(cb[1].qty == q1);
    CHECK(cb[1].price == p1);

    CHECK(cb[2].type == Book::TypedCallback::cb_trade);
    CHECK(cb[2].qty == q2);
    CHECK(cb[2].price == p2);

    CHECK(cb[3].type == Book::TypedCallback::cb_trade);
    CHECK(cb[3].qty == q - q1 - q2);
    CHECK(cb[3].price == p3);


    CHECK(book.bids().size() == 1);
    CHECK(book.asks().size() == 0);
  }


  SUBCASE("market buy against 3 limit sells") {
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



  SUBCASE("limit buy against limit sell") {
    const double p1 = 1000.00;

    const double q1 = 1.0;

    book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, p1, q1, 0));

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 1);

    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_2, BUY, p1, q1, 0));

    CHECK(cb.size() == 3);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[1].type == Book::TypedCallback::cb_trade);
    CHECK(cb[2].type == Book::TypedCallback::cb_book_update);

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 0);
  }


  SUBCASE("limit sell against limit buy") {
    const double p1 = 1000.00;

    const double q1 = 1.0;

    book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, p1, q1, 0));

    CHECK(book.bids().size() == 1);
    CHECK(book.asks().size() == 0);

    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, p1, q1, 0));

    CHECK(cb.size() == 3);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[1].type == Book::TypedCallback::cb_trade);
    CHECK(cb[2].type == Book::TypedCallback::cb_book_update);

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 0);
  }

}

}