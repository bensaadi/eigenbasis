#pragma once

#include <memory>
#include <book/ob.h>

#include <depth/depth_book.h>

#include "order.h"

namespace fixtures {

template <class Tracker, class... Plugins>
class ME : public book::OB<Tracker, Plugins...> {
public:
  typedef typename Tracker::OrderPtr OrderPtr;
  typedef std::vector<typename book::OB<Tracker, Plugins...>::TypedCallback> Callbacks;

  ME(uint32_t symbol_id);
  Callbacks add_and_get_cbs(const OrderPtr& order);

  void on_callbacks(const Callbacks& callbacks);

  void start_recording_callbacks() {
    recording_callbacks_ = true;
    Callbacks callbacks;
    callbacks.swap(recorded_callbacks_);
  }

  Callbacks get_recorded_callbacks() {
    Callbacks callbacks;
    callbacks.swap(recorded_callbacks_);
    return callbacks;
  }

private:
  bool recording_callbacks_;
  Callbacks callbacks_;
  Callbacks recorded_callbacks_;
};

template <class Tracker, class... Plugins>
ME<Tracker, Plugins...>::ME(uint32_t symbol_id)
: book::OB<Tracker, Plugins...>(symbol_id), recording_callbacks_(false) {

}


template <class Tracker, class... Plugins>
typename ME<Tracker, Plugins...>::Callbacks ME<Tracker, Plugins...>::add_and_get_cbs(const OrderPtr& order) {
  book::OB<Tracker, Plugins...>::add(order);
  assert(callbacks_.size() != 0);
  return callbacks_;
}

template <class Tracker, class... Plugins>
void ME<Tracker, Plugins...>::on_callbacks(const ME<Tracker, Plugins...>::Callbacks& callbacks) {
  callbacks_ = callbacks;

  if(recording_callbacks_) {
    for(auto cb : callbacks) {
      recorded_callbacks_.push_back(cb);
    }
  }
}

}