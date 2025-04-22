#include <cstdint>
#include <expected>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include "Cache.h"
#include "MemoryManager.h"
#include "MultiLevelCacheConfig.h"

static constexpr void parseParameters(const int argc, char** argv,
                                      std::string& traceFilePath,
                                      bool& enablePrefetch, bool& enableFifo,
                                      bool& enableVictimCache) {
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
        case 'p': {
          enablePrefetch = true;
          break;
        }
        case 'f': {
          enableFifo = true;
          break;
        }
        case 'v': {
          enableVictimCache = true;
          break;
        }
        default:
      }
    } else {
      if (traceFilePath.empty()) {
        traceFilePath = std::string(argv[i]);
      }
    }
  }
}

class CacheHierarchy {
  MemoryManager memoryManager;
  Cache l3Cache;
  Cache l2Cache;
  Cache l1Cache;
  bool enablePrefetch{false};
  bool enableFifo{false};
  bool enableVictimCache{false};

  struct PrefetchStatistics {
    bool isPrefetching{false};

    int32_t stride{0};
    uint32_t sameStrideCount{0};
    uint32_t diffStrideCount{0};
    uint32_t lastAccessAddress{0};
  };

  PrefetchStatistics prefetchStatistics{};

  static void outputCacheStats(std::ofstream& csvFile, const std::string& level,
                               const Cache* cache) {
    const auto& [numRead, numWrite, numHit, numMiss, totalCycles] =
        cache->getStatistics();
    const auto totalAccesses = numHit + numMiss;
    const auto missRate = totalAccesses > 0
                              ? static_cast<float>(numMiss) /
                                    static_cast<float>(totalAccesses) * 100.0f
                              : 0.0f;

    csvFile << std::format("{},{},{},{},{},{:.2f},{}\n", level, numRead,
                           numWrite, numHit, numMiss, missRate, totalCycles);
  }

 public:
  CacheHierarchy(const bool enablePrefetch, const bool enableFifo,
                 const bool enableVictimCache)
      : l3Cache(&memoryManager, MultiLevelCacheConfig::L3, nullptr),
        l2Cache(&memoryManager, MultiLevelCacheConfig::L2, &l3Cache),
        l1Cache(&memoryManager, MultiLevelCacheConfig::L1, &l2Cache),
        enablePrefetch(enablePrefetch),
        enableFifo(enableFifo),
        enableVictimCache(enableVictimCache) {}

  void processMemoryAccess(const char operation, const uint32_t addr) {
    if (!memoryManager.isPageExist(addr)) {
      memoryManager.addPage(addr);
    }

    if (enablePrefetch) {
      if (prefetchStatistics.isPrefetching) {
        const auto prefetchAddr = addr + prefetchStatistics.stride;
        if (!memoryManager.isPageExist(prefetchAddr)) {
          memoryManager.addPage(prefetchAddr);
        }
        l1Cache.fetch(prefetchAddr);
      }

      if (const auto currentStride = static_cast<int32_t>(
              static_cast<int64_t>(addr) -
              static_cast<int64_t>(prefetchStatistics.lastAccessAddress));
          currentStride == prefetchStatistics.stride) {
        ++prefetchStatistics.sameStrideCount;
        prefetchStatistics.diffStrideCount = 0;
      } else {
        ++prefetchStatistics.diffStrideCount;
        prefetchStatistics.sameStrideCount = 0;
        prefetchStatistics.stride = currentStride;
      }

      if (prefetchStatistics.sameStrideCount > 3) {
        prefetchStatistics.isPrefetching = true;
      }
      if (prefetchStatistics.diffStrideCount > 3) {
        prefetchStatistics.isPrefetching = false;
      }

      prefetchStatistics.lastAccessAddress = addr;
    }

    switch (operation) {
      case 'r': {
        l1Cache.read(addr);
        break;
      }
      case 'w': {
        l1Cache.write(addr, 0);
        break;
      }
      default: {
        throw std::runtime_error("Illegal memory access operation");
      }
    }
  }

  void outputResults(const std::string& traceFilePath) const {
    std::cout << "\n=== Cache Hierarchy Statistics ===\n";
    l1Cache.printStatistics();

    const std::string csvPath = traceFilePath + "_multi_level.csv";
    std::ofstream csvFile(csvPath);

    csvFile
        << "Level,NumReads,NumWrites,NumHits,NumMisses,MissRate,TotalCycles\n";

    // Write statistic into the table
    outputCacheStats(csvFile, "L1", &l1Cache);
    // outputCacheStats(csvFile, "L2", &l2Cache);
    // outputCacheStats(csvFile, "L3", &l3Cache);

    csvFile.close();
    std::cout << std::format("\nResults have been written to {}\n", csvPath);
  }
};

auto main(const int argc, char** argv) -> int {
  std::string traceFilePath;
  bool enablePrefetch = false;
  bool enableFifo = false;
  bool enableVictimCache = false;

  parseParameters(argc, argv, traceFilePath, enablePrefetch, enableFifo,
                  enableVictimCache);

  std::ifstream trace(traceFilePath);
  if (!trace.is_open()) {
    std::cerr << std::format("Unable to open file {}\n", traceFilePath);
    return -1;
  }

  try {
    CacheHierarchy cacheHierarchy{enablePrefetch, enableFifo,
                                  enableVictimCache};
    char operation = 0;
    uint32_t addr = 0;

    while (trace >> operation >> std::hex >> addr) {
      cacheHierarchy.processMemoryAccess(operation, addr);
    }

    cacheHierarchy.outputResults(traceFilePath);
  } catch (const std::exception& e) {
    std::cerr << std::format("Error: {}\n", e.what());
    return -1;
  }

  return 0;
}
