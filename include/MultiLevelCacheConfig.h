#ifndef MULTI_LEVEL_CACHE_CONFIG_H
#define MULTI_LEVEL_CACHE_CONFIG_H

#include "Cache.h"

class MultiLevelCacheConfig {
 public:
  static auto getL1Policy() -> Cache::Policy {
    return Cache::Policy{.cacheSize = 16 * 1024,  // 16KB
                         .blockSize = 64,
                         .blockNum = 16 * 1024 / 64,
                         .associativity = 1,  // direct-mapped
                         .hitLatency = 1,
                         .missLatency = 8};
  }

  static auto getL2Policy() -> Cache::Policy {
    Cache::Policy policy;
    policy.cacheSize = 128 * 1024;  // 128KB
    policy.blockSize = 64;
    policy.blockNum = policy.cacheSize / policy.blockSize;
    policy.associativity = 8;  // 8-way
    policy.hitLatency = 8;
    policy.missLatency = 20;
    return policy;
  }

  static Cache::Policy getL3Policy() {
    Cache::Policy policy;
    policy.cacheSize = 2048 * 1024;  // 2MB
    policy.blockSize = 64;
    policy.blockNum = policy.cacheSize / policy.blockSize;
    policy.associativity = 16;  // 16-way
    policy.hitLatency = 20;
    policy.missLatency = 100;
    return policy;
  }
};

#endif
