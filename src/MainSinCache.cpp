#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Cache.h"
#include "Debug.h"
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

  for (uint32_t cacheSize = 4 * 1024; cacheSize <= 1024 * 1024;
       cacheSize *= 4) {
    for (uint32_t blockSize = 32; blockSize <= 256; blockSize *= 2) {
      for (uint32_t associativity = 2; associativity <= 32;
           associativity *= 2) {
        const uint32_t blockNum = cacheSize / blockSize;
        if (blockNum % associativity != 0) {
          continue;
        }

        simulateCache(csvFile, cacheSize, blockSize, associativity);
      }
    }
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
      createSingleLevelPolicy(cacheSize, blockSize, associativity);

  MemoryManager *memory = nullptr;
  Cache *cache = nullptr;
  memory = new MemoryManager();
  cache = new Cache(memory, policy, nullptr);
  memory->setCache(cache);
  cache->printInfo(false);

  // Read and execute trace in cache-trace/ folder
  std::ifstream trace(traceFilePath);
  if (!trace.is_open()) {
    printf("Unable to open file %s\n", traceFilePath);
    exit(-1);
  }

  char op = 0;  //'r' for read, 'w' for write
  uint32_t addr = 0;
  char instType = 'I';  // 'I' for instruction, 'D' for data
  while (trace >> op >> std::hex >> addr >> instType) {
    if (verbose) {
      printf("%c %x\n", op, addr);
    }
    if (!memory->isPageExist(addr)) {
      memory->addPage(addr);
    }
    switch (op) {
      case 'r':
        cache->getByte(addr);
        break;
      case 'w':
        cache->setByte(addr, 0);
        break;
      default:
        dbgprintf("Illegal op %c\n", op);
        exit(-1);
    }

    if (verbose) {
      cache->printInfo(true);
    }

    if (isSingleStep) {
      printf("Press Enter to Continue...");
      getchar();
    }
  }

  // Output Simulation Results
  cache->printStatistics();
  const auto missRate = static_cast<float>(cache->statistics.numMiss) /
                        (cache->statistics.numHit + cache->statistics.numMiss);
  csvFile << cacheSize << "," << blockSize << "," << associativity << ","
          << missRate << "," << cache->statistics.totalCycles << std::endl;

  delete cache;
  delete memory;
}
