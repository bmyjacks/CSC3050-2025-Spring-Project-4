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

static constexpr auto parseParameters(const int argc, char **argv)
    -> std::expected<std::string, std::string> {
  if (argc > 1) {
    return std::string(argv[1]);
  }
  return std::unexpected("Usage: CacheSim trace-file\n");
}

class CacheHierarchy {
  std::shared_ptr<MemoryManager> memoryManager;
  std::unique_ptr<Cache> l1Cache;
  std::unique_ptr<Cache> l2Cache;
  std::unique_ptr<Cache> l3Cache;

  void outputCacheStats(std::ofstream &csvFile,
                        const std::string &level) const {
    const auto &[numRead, numWrite, numHit, numMiss, totalCycles] =
        l1Cache->getStatistics();
    const auto missRate = static_cast<float>(numMiss) /
                          static_cast<float>(numHit + numMiss) * 100.F;

    csvFile << std::format("{}, {}, {}, {}, {}, {}, {}\n", level, numRead,
                           numWrite, numHit, numMiss, missRate, totalCycles);
  }

 public:
  // Default constructor
  CacheHierarchy() {
    memoryManager = std::make_shared<MemoryManager>();

    const auto l1policy = MultiLevelCacheConfig::getL1Policy();
    const auto l2policy = MultiLevelCacheConfig::getL2Policy();
    const auto l3policy = MultiLevelCacheConfig::getL3Policy();

    l3Cache = std::make_unique<Cache>(memoryManager, l3policy, nullptr);
    l2Cache =
        std::make_unique<Cache>(memoryManager, l2policy, std::move(l3Cache));
    l1Cache =
        std::make_unique<Cache>(memoryManager, l1policy, std::move(l2Cache));
  }

  void processMemoryAccess(const char operation, const uint32_t addr) const {
    if (!memoryManager->isPageExist(addr)) {
      memoryManager->addPage(addr);
    }

    switch (operation) {
      case 'r': {
        l1Cache->read(addr);
        break;
      }
      case 'w': {
        l1Cache->write(addr, 0);
        break;
      }
      default: {
        throw std::runtime_error("Illegal memory access operation");
      }
    }
  }

  void outputResults(const std::string &traceFilePath) const {
    std::cout << "\n=== Cache Hierarchy Statistics ===\n";
    l1Cache->printStatistics();

    const std::string csvPath = traceFilePath + "_multi_level.csv";
    std::ofstream csvFile(csvPath);

    csvFile

        << "Level,NumReads,NumWrites,NumHits,NumMisses,MissRate,TotalCycles\n";

    // Just write L1 statistic into the table
    outputCacheStats(csvFile, "L1");

    csvFile.close();
    std::cout << std::format("\nResults have been written to {}\n", csvPath);
  }
};

auto main(const int argc, char **argv) -> int {
  auto traceFilePath = parseParameters(argc, argv);
  if (!traceFilePath) {
    std::print(std::cerr, "{}\n", traceFilePath.error());
    return -1;
  }

  std::ifstream trace(*traceFilePath);
  if (!trace.is_open()) {
    std::cerr << std::format("Unable to open file {}\n", *traceFilePath);
    return -1;
  }

  try {
    const CacheHierarchy cacheHierarchy;
    char operation = 0;
    uint32_t addr = 0;

    while (trace >> operation >> std::hex >> addr) {
      cacheHierarchy.processMemoryAccess(operation, addr);
    }

    cacheHierarchy.outputResults(*traceFilePath);
  } catch (const std::exception &e) {
    std::cerr << std::format("Error: {}\n", e.what());
    return -1;
  }

  return 0;
}
