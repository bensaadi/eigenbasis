#include <doctest/doctest.h>
#include <memory>
#include <cmath>

#include <book/types.h>
#include <book/plugins/positions.h>
#include "fixtures/order.h"
#include "fixtures/me.h"
#include "fixtures/helpers.h"

#define SYMBOL_ID_1 1
#define USER_1 1
#define USER_2 2

#define BUY true
#define SELL false


typedef fixtures::OrderWithUserID Order;
typedef std::shared_ptr<Order> OrderPtr;

struct Tracker :
  public virtual book::BaseTracker<OrderPtr>,
  public book::plugins::PositionsTracker<OrderPtr>
{
  Tracker(const OrderPtr& order) :
    book::BaseTracker<OrderPtr>(order),
    book::plugins::PositionsTracker<OrderPtr>(order) {}
};

typedef fixtures::ME<
  Tracker,
  book::plugins::PositionsPlugin<Tracker>
> Book;


TEST_CASE("positions") {
  Book book(SYMBOL_ID_1);

  double qty = 1.0;
  double qty2 = 2.0;
  double price = 1000.00;
  double price2 = 2000.00;

  SUBCASE("open a position") {
    book.add_and_get_cbs(std::make_shared<Order>(USER_2, BUY, price, qty, 0));
    Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, price, qty, 0));

    CHECK(cb.size() == 5);
    CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
    CHECK(cb[1].type == Book::TypedCallback::cb_trade);
    CHECK(cb[2].type == Book::TypedCallback::cb_position_open);
    CHECK(cb[3].type == Book::TypedCallback::cb_position_open);
    CHECK(cb[4].type == Book::TypedCallback::cb_book_update);

    /* long */
    CHECK(cb[2].user_id == USER_2);
    CHECK(cb[2].qty == qty);
    CHECK(cb[2].avg_price == price);

    /* short */
    CHECK(cb[3].user_id == USER_1);
    CHECK(cb[3].qty == -qty);
    CHECK(cb[3].avg_price == price);

    SUBCASE("increase a position") {
      book.add_and_get_cbs(std::make_shared<Order>(USER_2, BUY, price2, qty2, 0));
      Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, price2, qty2, 0));

      CHECK(cb.size() == 5);
      CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
      CHECK(cb[1].type == Book::TypedCallback::cb_trade);
      CHECK(cb[2].type == Book::TypedCallback::cb_position_update);
      CHECK(cb[3].type == Book::TypedCallback::cb_position_update);
      CHECK(cb[4].type == Book::TypedCallback::cb_book_update);

      /* long */
      CHECK(cb[2].user_id == USER_2);
      CHECK(cb[2].qty == qty + qty2);
      CHECK(cb[2].avg_price == (qty * price + qty2 * price2)/(qty + qty2));

      /* short */
      CHECK(cb[3].user_id == USER_1);
      CHECK(cb[3].qty == -(qty + qty2));
      CHECK(cb[3].avg_price == (qty * price + qty2 * price2)/(qty + qty2));

      SUBCASE("decrease a position") {
        book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, price2, qty2, 0));
        Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, price2, qty2, 0));

        CHECK(cb.size() == 5);
        CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
        CHECK(cb[1].type == Book::TypedCallback::cb_trade);
        CHECK(cb[2].type == Book::TypedCallback::cb_position_update);
        CHECK(cb[3].type == Book::TypedCallback::cb_position_update);
        CHECK(cb[4].type == Book::TypedCallback::cb_book_update);

        CHECK(cb[2].user_id == USER_2);
        CHECK(cb[2].qty == qty);
        CHECK(cb[2].avg_price == (qty * price + qty2 * price2)/(qty + qty2));

        CHECK(cb[3].user_id == USER_1);
        CHECK(cb[3].qty == -qty);
        CHECK(cb[3].avg_price == (qty * price + qty2 * price2)/(qty + qty2));

        SUBCASE("close a position") {
          book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, price, qty, 0));
          Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, price, qty, 0));

          CHECK(cb.size() == 5);
          CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
          CHECK(cb[1].type == Book::TypedCallback::cb_trade);
          CHECK(cb[2].type == Book::TypedCallback::cb_position_close);
          CHECK(cb[3].type == Book::TypedCallback::cb_position_close);
          CHECK(cb[4].type == Book::TypedCallback::cb_book_update);

          CHECK(cb[2].user_id == USER_2);
          CHECK(cb[3].user_id == USER_1);
        }

        SUBCASE("close a position and open an opposite position") {
          book.add_and_get_cbs(std::make_shared<Order>(USER_2, SELL, price2, qty2, 0));
          Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, BUY, price2, qty2, 0));

          CHECK(cb.size() == 7);
          CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
          CHECK(cb[1].type == Book::TypedCallback::cb_trade);
          CHECK(cb[2].type == Book::TypedCallback::cb_position_close);
          CHECK(cb[3].type == Book::TypedCallback::cb_position_open);
          CHECK(cb[4].type == Book::TypedCallback::cb_position_close);
          CHECK(cb[5].type == Book::TypedCallback::cb_position_open);
          CHECK(cb[6].type == Book::TypedCallback::cb_book_update);

          CHECK(cb[2].user_id == USER_2);
          CHECK(cb[4].user_id == USER_1);


          CHECK(cb[3].user_id == USER_2);
          CHECK(cb[3].qty == -(qty2 - qty));
          CHECK(cb[3].avg_price == price2);

          CHECK(cb[5].user_id == USER_1);
          CHECK(cb[5].qty == qty2 - qty);
          CHECK(cb[5].avg_price == price2);

          SUBCASE("close a position again") {
            book.add_and_get_cbs(std::make_shared<Order>(USER_2, BUY, price2, qty2 - qty, 0));
            Book::Callbacks cb = book.add_and_get_cbs(std::make_shared<Order>(USER_1, SELL, price2, qty2 - qty, 0));

            CHECK(cb.size() == 5);
            CHECK(cb[0].type == Book::TypedCallback::cb_order_accept);
            CHECK(cb[1].type == Book::TypedCallback::cb_trade);
            CHECK(cb[2].type == Book::TypedCallback::cb_position_close);
            CHECK(cb[3].type == Book::TypedCallback::cb_position_close);
            CHECK(cb[4].type == Book::TypedCallback::cb_book_update);

            CHECK(cb[2].user_id == USER_2);
            CHECK(cb[3].user_id == USER_1);
          }
        }
      }

    }
  }

}