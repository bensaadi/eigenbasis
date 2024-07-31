#pragma once

#include <vector>
#include "../transport/flatbuffers/depth_level_generated.h"

namespace depth {

typedef struct DepthLevelUpdatePair {
  uint32_t symbol_id;
  uint64_t change_id;
  int precision;
  std::vector<me::DepthLevel> levels;
} DepthLevelUpdatePair;

}