#include <doctest/doctest.h>
#include <memory>
#include <cmath>

#include <book/types.h>
#include <book/plugins/positions.h>
#include <book/plugins/reduce_only.h>
#include "fixtures/order.h"
#include "fixtures/me.h"
#include "fixtures/helpers.h"

#define SYMBOL_ID_1 1
#define USER_1 1
#define USER_2 2

#define BUY true
#define SELL false

using namespace book::plugins;

namespace reduce_only_test {

typedef fixtures::OrderWithReduceOnly Order;
typedef std::shared_ptr<Order> OrderPtr;

struct Tracker :
  public virtual book::BaseTracker<OrderPtr>,
  public book::plugins::PositionsTracker<OrderPtr>,
  public book::plugins::ReduceOnlyTracker<OrderPtr>
{
  Tracker(const OrderPtr& order) :
    book::BaseTracker<OrderPtr>(order),
    book::plugins::PositionsTracker<OrderPtr>(order),
    book::plugins::ReduceOnlyTracker<OrderPtr>(order) {}
};

typedef fixtures::ME<
  Tracker,
  book::plugins::PositionsPlugin<Tracker>,
  book::plugins::ReduceOnlyPlugin<Tracker>
> Book;


TEST_CASE("reduce-only") {
  Book book(SYMBOL_ID_1);

  double qty = 1.0;
  double qty2 = 2.0;
  double price = 1000.00;
  double price2 = 2000.00;
  double price3 = 3000.00;

  SUBCASE("add reduce-only with no open position should be rejected") {
    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, price, qty, 0, true));

    CHECK(cb.size() == 1);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_reject);
    CHECK(cb[0].reason == book::InsertRejectReasons::reduce_only_increase);
  }

  GIVEN("a short position") {
    book.add_and_get_cbs(std::make_shared<Order>(USER_2, BUY, price, qty, 0));
    book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, price, qty, 0));

    SUBCASE("add reduce-only with qty > position should be rejected") {
      Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, price, qty2, 0, true));

      CHECK(cb.size() == 1);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_reject);
      CHECK(cb[0].reason == book::InsertRejectReasons::reduce_only_increase);
    }

    SUBCASE("add reduce-only with qty = position should match") {
      book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, price, qty, 0));

      Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, price, qty, 0, true));

      CHECK(cb.size() == 5);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[2].type == Book::TypedCallback::cb_position_close);
      CHECK(cb[3].type == Book::TypedCallback::cb_position_close);
      CHECK(cb[4].type == Book::TypedCallback::cb_book_update);
    }


    SUBCASE("closing the position, having a pending reduce-only. should cancel the reduce-only") {
      book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, price, qty, 0, true));
      book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, price2, qty, 0));
      Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, 0, qty, 0));

      CHECK(cb.size() == 6);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[2].type == Book::TypedCallback::cb_position_close);
      CHECK(cb[3].type == Book::TypedCallback::cb_position_close);
      CHECK(cb[4].type == Book::TypedCallback::cb_order_cancel);
      CHECK(cb[5].type == Book::TypedCallback::cb_book_update);
    }

    SUBCASE("closing the position, having a pending reduce-only. should replace the reduce-only") {

      GIVEN("a reduce-only order that will trade later") {
        book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, price, qty, 0, true));
        
        GIVEN("the short position is decreased") {

          book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, price2, qty/2, 0));
          book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, price2, qty/2, 0));

          SUBCASE("the reduce-only order matches. should be replaced") {
            Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, price, qty, 0));
            CHECK(cb.size() == 7);
            CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
            CHECK(cb[1].type == Book::TypedCallback::cb_order_replace);
            CHECK(cb[2].type == Book::TypedCallback::cb_book_update);
            CHECK(cb[3].type == Book::TypedCallback::cb_trade);
            CHECK(cb[4].type == Book::TypedCallback::cb_position_close);
            CHECK(cb[5].type == Book::TypedCallback::cb_position_close);
            CHECK(cb[6].type == Book::TypedCallback::cb_book_update);
          }
        }
      }
    }


  }
}


}