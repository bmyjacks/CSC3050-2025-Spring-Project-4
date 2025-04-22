#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Cache.h"
#include "MemoryManager.h"

class InstructionCache final : public Cache {
 public:
  InstructionCache(MemoryManager *memoryManager, const Policy &policy,
                   Cache *lowerCache)
      : Cache(memoryManager, policy, lowerCache) {}

  void writeBlockToLowerLevel(Block &block) override {};
};

class DataCache final : public Cache {
 public:
  DataCache(MemoryManager *memoryManager, const Policy &policy,
            Cache *lowerCache)
      : Cache(memoryManager, policy, lowerCache) {}
};

static bool verbose = false;
static bool isSingleStep = false;
static std::string traceFilePath;

static auto parseParameters(const int argc, char **argv) -> bool {
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
        case 'v': {
          verbose = true;
          break;
        }
        case 's': {
          isSingleStep = true;
          break;
        }
        default: {
          return false;
        }
      }
    } else {
      if (traceFilePath.empty()) {
        traceFilePath = std::string(argv[i]);
      } else {
        return false;
      }
    }
  }

  return !traceFilePath.empty();
}

void printUsage() {
  std::print("Usage: CacheSim trace-file [-s] [-v]\n");
  std::print("Parameters: -s single step, -v verbose output\n");
}

static auto createSingleLevelPolicy(const uint32_t cacheSize,
                                    const uint32_t blockSize,
                                    const uint32_t associativity)
    -> Cache::Policy {
  return Cache::Policy{.cacheSize = cacheSize,
                       .blockSize = blockSize,
                       .blockNum = cacheSize / blockSize,
                       .associativity = associativity,
                       .hitLatency = 1,
                       .missLatency = 100};
}

void simulateCache(std::ofstream &csvFile, const uint32_t cacheSize,
                   const uint32_t blockSize, const uint32_t associativity) {
  const auto policy =
      createSingleLevelPolicy(cacheSize >> 1U, blockSize, associativity);

  auto memoryManager = MemoryManager();
  auto instCache = InstructionCache(&memoryManager, policy, nullptr);
  auto dataCache = DataCache(&memoryManager, policy, nullptr);

  std::print("=== Instruction Cache ===\n");
  instCache.printInfo(verbose);

  std::print("\n=== Data Cache ===\n");
  dataCache.printInfo(verbose);

  auto cacheOperation = [](Cache &cache, const char &operation,
                           const uint32_t &addr) {
    switch (operation) {
      case 'r': {
        cache.read(addr);
        break;
      }
      case 'w': {
        cache.write(addr, 0);
        break;
      }
      default: {
        throw std::runtime_error(std::format(
            "Illegal operation {} to address 0x{:x}", operation, addr));
      }
    }
  };

  // Read and execute trace in cache-trace/ folder
  std::ifstream trace(traceFilePath);
  if (!trace.is_open()) {
    throw std::runtime_error(
        std::format("Unable to open file {}", traceFilePath));
  }

  char operation = 0;  //'r' for read, 'w' for write
  uint32_t addr = 0;
  char instType = 'I';  // 'I' for instruction, 'D' for data
  while (trace >> operation >> std::hex >> addr >> instType) {
    if (verbose) {
      std::print("Operation: {} Address: 0x{:x} Type: {}\n", operation, addr,
                 instType);
    }

    if (!memoryManager.isPageExist(addr)) {
      memoryManager.addPage(addr);
    }

    switch (instType) {
      case 'I': {
        cacheOperation(instCache, operation, addr);
        break;
      }
      case 'D': {
        cacheOperation(dataCache, operation, addr);
        break;
      }
      default: {
        throw std::runtime_error(std::format(
            "Illegal instruction type {} to address 0x{:x}", instType, addr));
      }
    }

    if (verbose) {
      instCache.printInfo(true);
      dataCache.printInfo(true);
    }

    if (isSingleStep) {
      std::print("Press Enter to Continue...");
      std::cin.get();
    }
  }

  // Output Simulation Results
  std::print("=== Instruction Cache ===\n");
  instCache.printStatistics();
  std::print("\n=== Data Cache ===\n");
  dataCache.printStatistics();

  const auto missCycles =
      instCache.getStatistics().numMiss + dataCache.getStatistics().numMiss;
  const auto totalCycles = std::max(instCache.getStatistics().totalCycles,
                                    dataCache.getStatistics().totalCycles);

  const auto missRate =
      static_cast<float>(missCycles) / static_cast<float>(totalCycles);

  csvFile << std::format("{}, {}, {}, {}, {}\n", cacheSize, blockSize,
                         associativity, missRate, totalCycles);
}

auto main(const int argc, char **argv) -> int {
  if (!parseParameters(argc, argv)) {
    printUsage();
    return -1;
  }

  // Open CSV file and write header
  std::ofstream csvFile(std::string(traceFilePath) + ".csv");
  csvFile << "cacheSize,blockSize,associativity,missRate,totalCycles\n";

  constexpr auto cacheSize = 16 * 1024;  // 16KB
  constexpr auto blockSize = 64;         // 64B
  constexpr auto associativity = 1;      // Direct-mapped

  try {
    simulateCache(csvFile, cacheSize, blockSize, associativity);
  } catch (const std::exception &e) {
    std::print(std::cerr, "Error: {}\n", e.what());
    return -1;
  }

  std::print("Result has been written to {}\n",
             std::string(traceFilePath) + ".csv");
  csvFile.close();

  return 0;
}
