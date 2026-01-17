#pragma once

#include <book/plugin.h>
#include <book/book_price.h>

namespace book {
namespace plugins {

struct StopOrder {
  virtual double stop_price() const = 0;
};

template <class Tracker>
class StopOrdersPlugin : public Plugin<Tracker> {
public:
	using OrderPtr = typename Plugin<Tracker>::OrderPtr;
	using TrackerMap = typename Plugin<Tracker>::TrackerMap;
	using TrackerVec = typename Plugin<Tracker>::TrackerVec;
	using TypedCallback = typename Plugin<Tracker>::TypedCallback;

protected:
	bool should_add_tracker(const Tracker& taker) override {
		double stop_price = taker.ptr()->stop_price();
		return stop_price == 0 || add_stop_order(taker, stop_price);
	}

	void on_market_price_change(double prev_price, double new_price) override {
		if(prev_price == new_price) return;
		auto& trackers = new_price > prev_price ? stop_bids_ : stop_asks_;
		BookPrice until(new_price > prev_price, new_price);
		check_stop_orders(trackers, until);
	}

  void after_add_tracker(const Tracker& taker) override {
  	while(!pending_orders_.empty())
      submit_pending_orders();
  }


private:
	TrackerMap stop_bids_;
	TrackerMap stop_asks_;
	TrackerVec pending_orders_;

	bool add_stop_order(const Tracker& tracker, double stop_price) {
	  bool is_bid = tracker.is_bid();
	  BookPrice key(is_bid, stop_price);

	  /* triggered */
	  if(key >= this->market_price()) return false;

    if(is_bid)
      stop_bids_.emplace(key, tracker);
    else
      stop_asks_.emplace(key, tracker);

	  return true;
	}

	void check_stop_orders(TrackerMap& stops, BookPrice& until) {
		auto pos = stops.begin(); 
		while(pos != stops.end()) {
			auto here = pos++;
			Tracker & tracker = here->second;

			if(here->first < until)
				break;

			pending_orders_.push_back(std::move(tracker));
			stops.erase(here);
		}
	}

  void submit_pending_orders() {
	  TrackerVec pending;
	  pending.swap(pending_orders_);
	  for(auto pos = pending.begin(); pos != pending.end(); ++pos) {
	    Tracker& tracker = *pos;
	    this->add_tracker(tracker);
	    this->callbacks().push_back(TypedCallback::stop_trigger(tracker.ptr()));
	  }
  }
};

} // namespace plugins
} // namespace book