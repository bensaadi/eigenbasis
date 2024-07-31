#include <doctest/doctest.h>
#include <memory>
#include <cmath>

#include <book/types.h>
#include <book/plugins/self_trade_policy.h>
#include "fixtures/order.h"
#include "fixtures/me.h"
#include "fixtures/helpers.h"

#define SYMBOL_ID_1 1
#define USER_1 1
#define USER_2 2

#define BUY true
#define SELL false

namespace stp_test {

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


TEST_CASE("self trade policy") {
  Book book(SYMBOL_ID_1);

  SUBCASE("resting buy limit stp = taker") {
    auto order1 = std::make_shared<Order>(USER_1, BUY, 1000.00, 1.0, 0);
    Book::Callbacks cb = book.add_and_get_cbs(order1);

    CHECK(cb.size() == 2);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[0].qty == 0);

    CHECK(cb[0].avg_price == 0);

    CHECK(cb[1].type == Book::TypedCallback::cb_book_update);

    CHECK(book.bids().size() == 1);
    CHECK(book.asks().size() == 0);

    SUBCASE("sell limit self-trade cancel taker") {
      Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, 1000.00, 1.0, 0));

      CHECK(cb.size() == 3);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].generic_1 == 1);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);

      CHECK(book.bids().size() == 1);
      CHECK(book.asks().size() == 0);
    }

    SUBCASE("sell market self-trade cancel taker") {
      Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, 0, 1.0, 0));

      CHECK(cb.size() == 3);

      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].generic_1 == 0);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);

      CHECK(book.bids().size() == 1);
      CHECK(book.asks().size() == 0);
    }

    SUBCASE("sell limit self-trade cancel maker") {
      auto order2 = std::make_shared<Order>(USER_1, SELL, 1000.00, 1.0, 0);
      order2->stp(book::plugins::stp_cancel_maker);

      Book::Callbacks cb = book.add_and_get_cbs(order2);

      CHECK(cb.size() == 4);

      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].order == order1);
      CHECK(cb[1].generic_1 == 1);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);

      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].order == order2);
      CHECK(cb[2].generic_1 == 1);
      CHECK(cb[2].reason == (int)book::CancelReasons::self_trade);

      CHECK(cb[3].type == Book::TypedCallback::cb_book_update);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }

    SUBCASE("sell market self-trade cancel maker") {
      auto order2 = std::make_shared<Order>(USER_1, SELL, 0, 1.0, 0);
      order2->stp(book::plugins::stp_cancel_maker);

      Book::Callbacks cb = book.add_and_get_cbs(order2);

      CHECK(cb.size() == 4);

      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].order == order1);
      CHECK(cb[1].generic_1 == 1);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);


      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].order == order2);
      CHECK(cb[2].generic_1 == 0);
      CHECK(cb[2].reason == (int)book::CancelReasons::self_trade);

      CHECK(cb[3].type == Book::TypedCallback::cb_book_update);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }

    SUBCASE("sell limit self-trade cancel both") {
      auto order2 = std::make_shared<Order>(USER_1, SELL, 1000.00, 1.0, 0);
      order2->stp(book::plugins::stp_cancel_both);

      Book::Callbacks cb = book.add_and_get_cbs(order2);

      CHECK(cb.size() == 4);

      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].order == order1);
      CHECK(cb[1].generic_1 == 1);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);


      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].order == order2);
      CHECK(cb[2].generic_1 == 1);
      CHECK(cb[2].reason == (int)book::CancelReasons::self_trade);

      CHECK(cb[3].type == Book::TypedCallback::cb_book_update);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }
  }

  SUBCASE("resting buy limit stp = maker") {
    auto order1 = std::make_shared<Order>(USER_1, BUY, 1000.00, 1.0, 0);
    order1->stp(book::plugins::stp_cancel_maker);

    Book::Callbacks cb = book.add_and_get_cbs(order1);

    CHECK(cb.size() == 2);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[0].qty == 0);

    CHECK(cb[0].avg_price == 0);

    CHECK(cb[1].type == Book::TypedCallback::cb_book_update);

    CHECK(book.bids().size() == 1);
    CHECK(book.asks().size() == 0);

    SUBCASE("sell limit self-trade cancel maker") {
      auto order2 = std::make_shared<Order>(USER_1, SELL, 1000.00, 1.0, 0);
      order2->stp(book::plugins::stp_cancel_maker);

      Book::Callbacks cb = book.add_and_get_cbs(order2);

      CHECK(cb.size() == 3);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].order == order1);
      CHECK(cb[1].generic_1 == 1);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);

      CHECK(cb[2].type == Book::TypedCallback::cb_book_update);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 1);
    }

    SUBCASE("sell market self-trade cancel maker") {
      auto order2 = std::make_shared<Order>(USER_1, SELL, 0, 1.0, 0);
      order2->stp(book::plugins::stp_cancel_maker);

      Book::Callbacks cb = book.add_and_get_cbs(order2);

      CHECK(cb.size() == 4);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].order == order1);
      CHECK(cb[1].generic_1 == 1);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);

      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].order == order2);
      CHECK(cb[2].generic_1 == 0);
      CHECK(cb[2].reason == (int)book::CancelReasons::no_liquidity);

      CHECK(cb[3].type == Book::TypedCallback::cb_book_update);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }
  }

  SUBCASE("resting buy limit stp = both") {
    auto order1 = std::make_shared<Order>(USER_1, BUY, 1000.00, 1.0, 0);
    order1->stp(book::plugins::stp_cancel_both);

    Book::Callbacks cb = book.add_and_get_cbs(order1);

    CHECK(cb.size() == 2);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[0].qty == 0);

    CHECK(cb[0].avg_price == 0);

    CHECK(cb[1].type == Book::TypedCallback::cb_book_update);

    CHECK(book.bids().size() == 1);
    CHECK(book.asks().size() == 0);

    SUBCASE("sell limit self-trade cancel maker") {
      auto order2 = std::make_shared<Order>(USER_1, SELL, 1000.00, 1.0, 0);
      order2->stp(book::plugins::stp_cancel_maker);

      Book::Callbacks cb = book.add_and_get_cbs(order2);

      CHECK(cb.size() == 4);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].order == order1);
      CHECK(cb[1].generic_1 == 1);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);


      CHECK(cb[2].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[2].order == order2);
      CHECK(cb[2].generic_1 == 1);
      CHECK(cb[2].reason == (int)book::CancelReasons::self_trade);

      CHECK(cb[3].type == Book::TypedCallback::cb_book_update);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 0);
    }
  }

  SUBCASE("resting sell limit stp = taker") {
    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, 1000.00, 1.0, 0));

    CHECK(cb.size() == 2);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[0].qty == 0);

    CHECK(cb[0].avg_price == 0);

    CHECK(cb[1].type == Book::TypedCallback::cb_book_update);

    CHECK(book.bids().size() == 0);
    CHECK(book.asks().size() == 1);

    SUBCASE("buy limit self-trade cancel taker") {
      Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, 1000.00, 1.0, 0));

      CHECK(cb.size() == 3);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].generic_1 == 1);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 1);
    }

    SUBCASE("buy market self-trade cancel taker") {
      Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, 0, 0, 1000.00));

      CHECK(cb.size() == 3);

      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[0].qty == 0);

      CHECK(cb[0].avg_price == 0.0);

      CHECK(cb[1].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[1].generic_1 == 0);
      CHECK(cb[1].reason == (int)book::CancelReasons::self_trade);

      CHECK(book.bids().size() == 0);
      CHECK(book.asks().size() == 1);
    }
  }

}

}