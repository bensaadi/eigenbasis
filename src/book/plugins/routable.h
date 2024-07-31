/*
 *  Copyright (c) 2019-present, LBS Trading LLC. All rights reserved.
 *  See the file LICENSE.md for licensing information.
 */

/*
  The plugin enables asynchronous matching on an external exchange, then handles
  callbacks as if the match occured internally.

  - HOOK after_trade: if a trade involves a MM order,
    create a routing request for routing the order

  - HOOK should_trade: temporary cancels the taker order pending routing response

  - HOOK after_add_tracker: silently cancel the remaining qty of the user order,
    submit a routing request, and erase the callbacks related to the user order
  
  - on_routing_success: issue a fill callback (and possibly a cancel callback too
    if there is qty remaining), and add_tracker() the tracker with the remaining qty if any.

  - on_routing_failure: issue a cancel callback


  ASSUMPTIONS
  ===========

  - MM orders are POST-ONLY
  - MM orders are cancelled as soon as they match. Market maker is responsible for
    reposting the order
  - MM orders have post-trade settlement. Clearing should ignore all callbacks
    related to MM users


  CALLBACK SCOPES
  ===============

  - in hold updater :
    if(suppressed) return

  - in depth :
    if(not (internal_only or broadcast_to_all)) return

  - callbacks publisher
    if(not (external only or broadcast_to_all)) return
*/

#pragma once

#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>
#include <set>
#include <memory>

#include <book/types.h>
#include <book/tracker.h>
#include <book/constants.h>
#include <book/plugin.h>
#include <book/exceptions.h>
#include <book/types.h>
#include <book/callback.h>

#include <utils/uint128.h>
#include <utils/ts.h>

#include <book/plugins/trackers/user_id_tracker.h>

using namespace utils;

namespace book {
namespace plugins {

template <class OrderPtr>
struct RoutableTracker :
public virtual UserIDTracker<OrderPtr>, public virtual BaseTracker<OrderPtr> 
{
public:
  RoutableTracker(const OrderPtr& order) :
    BaseTracker<OrderPtr>(order) {
    UserIDTracker<OrderPtr>::set_user_id(order->user_id());
  }

  void fill(double fill_qty, double fill_cost) {
    BaseTracker<OrderPtr>::fill(fill_qty, fill_cost);
  }
};

template <class Tracker>
class RoutablePlugin :
public Plugin<Tracker> {
public:
  typedef typename Tracker::OrderPtr OrderPtr;
  typedef std::shared_ptr<Tracker> TrackerPtr;
  typedef Callback<OrderPtr> TypedCallback;

  struct RoutingRequest {
    uint64_t request_id;
    uint32_t exchange_id;
    uint32_t symbol_id;
    double qty;
    double price;
    bool is_bid;
    CancelReasons cancel_reason;
    TrackerPtr maker;
    TrackerPtr taker;
    std::list<TypedCallback> callbacks;
  };

  RoutablePlugin() {
    next_routing_request_.request_id = 0;
    reset_request();
  }

  void register_market_maker(uint64_t user_id, uint32_t external_exchange_id) {
    MMU2X_.emplace(user_id, external_exchange_id);
    X2MMU_.emplace(external_exchange_id, user_id);
  }

private:
  RoutingRequest next_routing_request_;
  std::unordered_map<uint64_t, RoutingRequest> pending_requests_;
  std::set<uint128> pending_maker_order_ids_;
  bool market_price_changed_;
  bool should_route_;

  /* translate MM user id on this exchange <-> external exchange id to route to */
  std::unordered_map<uint32_t, uint32_t> MMU2X_;
  std::unordered_map<uint32_t, uint32_t> X2MMU_;

protected:
  virtual void on_routing_request(const RoutingRequest& request) = 0;

  void reset_request() {
    next_routing_request_.callbacks.clear();
    next_routing_request_.qty = 0;
    next_routing_request_.cancel_reason = dont_cancel;
    market_price_changed_ = false;
    should_route_ = false;
  }

  void after_add_tracker(Tracker& taker) {
    if(!should_route_) return;

    /* cancel taker order.
      XXX: `taker` will become dangling, do not use in the rest of
      this function. use next_routing_request_.taker instead  */
    this->do_cancel(taker.ptr(), CancelReasons::temporary_cancel);

    /* remove fill callbacks related to MM matching */
    std::vector<Callback<OrderPtr>>& callbacks = this->callbacks();

    assert(callbacks.size() > 0);
    size_t start = callbacks.size() - 1;

    /* find the latest accept cb */
    while(start > 0)
      if(callbacks[start--].type == Callback<OrderPtr>::cb_order_accept)
        break;

    for(auto it = callbacks.begin() + start; it != callbacks.end(); ++it) {
      Callback<OrderPtr>& cb = *it;

      /* ignore if callback type is irrelevant */
      if(cb.type != Callback<OrderPtr>::cb_trade &&
        cb.type != Callback<OrderPtr>::cb_order_cancel) continue;

      /* ignore if order id is irrelevant */
      if(cb.order->order_id() !=
          next_routing_request_.taker->ptr()->order_id()) continue;

      /* ignore irrelevant trade with non-MM maker*/
      if(cb.type == Callback<OrderPtr>::cb_trade &&
        MMU2X_.find(cb.maker_order->user_id()) == MMU2X_.end()) continue;

      if(cb.type == Callback<OrderPtr>::cb_trade) {
        cb.scope = Callback<OrderPtr>::CbScope::internal_only;
        next_routing_request_.callbacks.push_back(cb);
      }
      
      /* save the fact that it was cancelled afterwards, if it was */
      else if(cb.type == Callback<OrderPtr>::cb_order_cancel) {
        cb.scope = Callback<OrderPtr>::CbScope::suppress_callback;
      }
    }

    /* submit the request */

    next_routing_request_.request_id = ts();
    auto emplaced = pending_requests_.emplace(
      next_routing_request_.request_id, next_routing_request_);
    
    reset_request();
    on_routing_request(emplaced.first->second);
  }

  void should_trade(
    Tracker& taker,
    Tracker& maker,
    CancelReasons& taker_reason,
    CancelReasons& maker_reason
  ) {
    auto exchange_id_it = MMU2X_.find(maker.user_id());
    if(exchange_id_it == MMU2X_.end()) {
      /* we've hit a user order. take care of the routing first */
      if(should_route_)
        taker_reason = temporary_cancel;
      return;
    }

    /* we've hit a maker order that is already routing -- cancel maker */
    if(pending_maker_order_ids_.count(maker.ptr()->order_id()) == 1) {
      maker_reason = mm_routed;
    }

    /* we've hit a different exchange -- cancel taker.
     * will be added again once it has been routed to first exchange */
    if(should_route_ && 
      next_routing_request_.exchange_id != exchange_id_it->second) {
      taker_reason = temporary_cancel;
    }
  }

  void after_trade(
    Tracker& taker,
    Tracker& maker,
    bool maker_is_bid,
    double qty,
    double price
  ) {
    auto exchange_id_it = MMU2X_.find(maker.user_id());
    /* skipping non-MM orders */
    if(exchange_id_it == MMU2X_.end()) {
      market_price_changed_ = true;
      return;
    }

    market_price_changed_ = false;
    pending_maker_order_ids_.insert(maker.ptr()->order_id());

    /* this calls copy ctor */
    next_routing_request_.taker = std::make_shared<Tracker>(taker);
    next_routing_request_.maker = std::make_shared<Tracker>(maker);

    next_routing_request_.exchange_id = exchange_id_it->second;
    next_routing_request_.symbol_id = this->symbol_id();
    next_routing_request_.qty += qty;

    /* after each trade, the price gets worse. so we're expected 
      to send the worst price on the request. */
    next_routing_request_.price = price;
    next_routing_request_.is_bid = !maker_is_bid;
    should_route_ = true;
  }

  void on_routing_success(uint64_t request_id) {
    RoutingRequest& request = pending_requests_.at(request_id);

    const TrackerPtr& maker = request.maker, taker = request.taker;

    /* replay callbacks */
    for(auto it = request.callbacks.begin(); it != request.callbacks.end(); ++it) {
      it->scope = Callback<OrderPtr>::CbScope::external_only;
      this->emit_callback(*it);
    }

    /* if there's qty remaining */
    if(request.cancel_reason == dont_cancel) {
      if(!taker->filled()) {
        /* add the tracker again */

        /* before adding the tracker again, we must first process the 
          callbacks related to this routing request, so as not to save 
          them on any subsequent request resulting from add_tracker */
        this->process_callbacks();
    
        this->add_tracker(*taker);
        size_t accept_cb_index = this->callbacks().size();
        this->emit_callback(TypedCallback::accept(taker->ptr()));

        /* only need this to have the order in directory and order list*/
        this->callbacks()[accept_cb_index].scope =
          Callback<OrderPtr>::CbScope::suppress_callback;
        this->process_callbacks();
      }
    }

    pending_maker_order_ids_.erase(maker->ptr()->order_id());
    this->process_callbacks();
  }

  void on_routing_failure(uint64_t request_id) {
    RoutingRequest& request = pending_requests_.at(request_id);

    const TrackerPtr& maker = request.maker, taker = request.taker;

    /* replay callbacks */
    for(auto it = request.callbacks.begin(); it != request.callbacks.end(); ++it) {
      /* dont replay the fill with this MM */
      if(it->type == Callback<OrderPtr>::cb_trade &&
        it->maker_order->user_id() == X2MMU_.at(request.exchange_id)) continue;

      it->scope = Callback<OrderPtr>::CbScope::external_only;
      this->emit_callback(*it);
    }

    size_t cancel_cb_index = this->callbacks().size();
    this->emit_cancel_callback(*taker, routing_failure);

    /* do not change depth again */
    this->callbacks()[cancel_cb_index].scope =
      Callback<OrderPtr>::CbScope::external_only;
    
    this->callbacks()[cancel_cb_index].qty -= request.qty;

    /* used to free the hold */
    this->callbacks()[cancel_cb_index].generic_1 = request.qty + taker->qty_on_book();

    pending_maker_order_ids_.erase(maker->ptr()->order_id());

    this->process_callbacks();
  }

};

}
}