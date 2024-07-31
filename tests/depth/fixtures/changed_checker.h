#include <depth/depth.h>
#include <cstdarg>
#include <iostream>

using depth::Depth;
using depth::DepthLevel;
using depth::ChangeId;

namespace test {

template <int SIZE = 30>
class ChangedChecker {
public:
  typedef Depth<SIZE> SizedDepth;
  ChangedChecker(SizedDepth& depth)
  : depth_(depth)
  {
    reset();
  }

  void reset() {
    last_change_ = depth_.last_change();
  }

  #define check_bid_changed(...) check_changed(true, 5, __VA_ARGS__)
  #define check_ask_changed(...) check_changed(false, 5, __VA_ARGS__)

  bool check_changed(bool is_bid, int num_levels, ...) {
    bool matched = true;
    va_list args;
    va_start(args, num_levels);

    for(int i = 0; i < num_levels; ++i) {
      bool level = va_arg(args, int);
      matched &= check_level(is_bid ? depth_.bids() : depth_.asks(), i, level);
    }

    va_end(args);
    return matched;
  }

  private:
  ChangeId last_change_;


  bool check_level(const DepthLevel* levels, size_t index, bool expected) {
    return levels[index].changed_since(last_change_) == expected;
  }

  private:
  SizedDepth& depth_;
};

}
