#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Cache.h"
#include "MemoryManager.h"

static bool verbose = false;
static bool isSingleStep = false;
static char *traceFilePath;

static auto parseParameters(int argc, char **argv) -> bool;
static void printUsage();
static void simulateCache(std::ofstream &csvFile, uint32_t cacheSize,
                          uint32_t blockSize, uint32_t associativity);

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
    std::cerr << std::format("Error: {}\n", e.what());
    return -1;
  }

  std::cout << std::format("Result has been written to {}\n",
                           std::string(traceFilePath) + ".csv");
  csvFile.close();
  return 0;
}

auto parseParameters(const int argc, char **argv) -> bool {
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
      if (traceFilePath == nullptr) {
        traceFilePath = argv[i];
      } else {
        return false;
      }
    }
  }

  return traceFilePath != nullptr;
}

void printUsage() {
  std::cout << "Usage: CacheSim trace-file [-s] [-v]\n";
  std::cout << "Parameters: -s single step, -v verbose output\n";
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

  auto memoryManager = std::make_shared<MemoryManager>();
  auto instCache = Cache(memoryManager, policy, nullptr);
  auto dataCache = Cache(memoryManager, policy, nullptr);

  instCache.printInfo(false);
  dataCache.printInfo(false);

  auto cacheOperation = [](Cache &cache, const char &operation,
                           const uint32_t &addr) {
    switch (operation) {
      case 'r': {
        cache.getByte(addr);
        break;
      }
      case 'w': {
        cache.setByte(addr, 0);
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
      std::cout << std::format("Operation: {} Address: 0x{:x} Type: {}\n",
                               operation, addr, instType);
    }

    if (!memoryManager->isPageExist(addr)) {
      memoryManager->addPage(addr);
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
      dataCache.printInfo(true);
    }

    if (isSingleStep) {
      std::cout << "Press Enter to Continue...";
      std::cin.get();
    }
  }

  // Output Simulation Results
  std::cout << "Instruction Cache Statistics:\n";
  instCache.printStatistics();
  std::cout << "Data Cache Statistics:\n";
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
