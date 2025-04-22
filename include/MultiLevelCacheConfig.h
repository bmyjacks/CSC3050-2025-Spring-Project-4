#ifndef MULTI_LEVEL_CACHE_CONFIG_H
#define MULTI_LEVEL_CACHE_CONFIG_H

#include "Cache.h"

namespace MultiLevelCacheConfig {
constexpr auto L1 = Cache::Policy{.cacheSize = 16 * 1024,  // 16KB
                                  .blockSize = 64,
                                  .blockNum = 16 * 1024 / 64,
                                  .associativity = 1,  // direct-mapped
                                  .hitLatency = 1,
                                  .missLatency = 8};
constexpr auto L2 = Cache::Policy{.cacheSize = 128 * 1024,  // 128KB
                                  .blockSize = 64,
                                  .blockNum = 128 * 1024 / 64,
                                  .associativity = 8,  // 8-way
                                  .hitLatency = 8,
                                  .missLatency = 20};
constexpr auto L3 = Cache::Policy{.cacheSize = 2048 * 1024,  // 2MB
                                  .blockSize = 64,
                                  .blockNum = 2048 * 1024 / 64,
                                  .associativity = 16,  // 16-way
                                  .hitLatency = 20,
                                  .missLatency = 100};
}  // namespace MultiLevelCacheConfig

#endif
