#ifndef CACHE_H
#define CACHE_H

#include <vector>

#include "MemoryManager.h"

class Cache {
 public:
  struct Policy {
    uint32_t cacheSize;      // In bytes, must be power of 2
    uint32_t blockSize;      // In bytes, must be power of 2
    uint32_t blockNum;       // Total number of blocks
    uint32_t associativity;  // Number of blocks per set
    uint32_t hitLatency;     // Latency for a cache hit
    uint32_t missLatency;    // Latency for a cache miss
  };

  struct Block {
    bool valid;                 // Whether the block is valid
    bool modified;              // Whether the block is modified
    uint32_t tag;               // Tag of the block
    uint32_t id;                // ID of the block
    uint32_t size;              // Size of the block in bytes
    uint32_t lastReference;     // Last reference timestamp
    std::vector<uint8_t> data;  // Data stored in the block
  };

  struct Statistics {
    uint32_t numRead;      // Number of read operations
    uint32_t numWrite;     // Number of write operations
    uint32_t numHit;       // Number of cache hits
    uint32_t numMiss;      // Number of cache misses
    uint32_t totalCycles;  // Total cycles spent
  };

  Cache(MemoryManager *manager, const Policy &policy, Cache *lowerCache);
  virtual ~Cache() = default;

  auto read(uint32_t addr) -> uint8_t;
  void write(uint32_t addr, uint8_t val);
  auto inCache(uint32_t addr) -> bool;
  [[nodiscard]] auto getBlockId(uint32_t addr) -> uint32_t;
  auto getByte(uint32_t addr) -> uint8_t;
  void setByte(uint32_t addr, uint8_t val);
  void printInfo(bool verbose) const;
  void printStatistics() const;
  void fetch(uint32_t addr);
  [[nodiscard]] auto getPolicy() const -> Policy { return policy; }
  [[nodiscard]] auto getStatistics() const -> Statistics { return statistics; }

 private:
  uint32_t referenceCounter;  // Reference counter for LRU
  MemoryManager *memoryManager;
  Cache *lowerCache;
  Policy policy;
  std::vector<Block> blocks;
  Statistics statistics;

  void loadBlockFromLowerLevel(uint32_t addr);
  [[nodiscard]] auto getReplacementBlockId(uint32_t begin, uint32_t end) const
      -> uint32_t;
  virtual void writeBlockToLowerLevel(Block &block);

  auto isPolicyValid() -> bool;
  static auto isPowerOfTwo(uint32_t n) -> bool;
  static auto log2i(uint32_t val) -> uint32_t;
  auto getTag(uint32_t addr) -> uint32_t;
  auto getId(uint32_t addr) -> uint32_t;
  auto getOffset(uint32_t addr) -> uint32_t;
  auto getAddr(Block &block) -> uint32_t;
};

#endif