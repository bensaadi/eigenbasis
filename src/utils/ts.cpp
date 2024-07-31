#include "ts.h"

namespace utils {


uint64_t ts() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}


}