#include "Cache.h"

#include <array>
#include <bit>
#include <format>
#include <iostream>
#include <ranges>

Cache::Cache(MemoryManager *manager, const Policy &policy, Cache *lowerCache)
    : referenceCounter(0),
      memoryManager(manager),
      lowerCache(lowerCache),
      victimCache(nullptr),
      policy(policy),
      statistics({.numRead = 0,
                  .numWrite = 0,
                  .numHit = 0,
                  .numMiss = 0,
                  .totalCycles = 0}),
      enableFifo(false),
      enableVictimCache(false) {
  if (!isPolicyValid()) {
    throw std::runtime_error("Invalid cache policy");
  }

  blocks.resize(policy.blockNum);
  for (const auto idx :
       std::views::iota(0U, static_cast<uint32_t>(blocks.size()))) {
    auto &[valid, modified, tag, id, size, lastReference, createdAt, data] =
        blocks[idx];
    valid = false;
    modified = false;
    tag = 0;
    id = idx / policy.associativity;
    size = policy.blockSize;
    lastReference = 0;
    createdAt = 0;
    data = std::vector<uint8_t>(size);
  }
}

[[nodiscard]] auto Cache::getStatistics() const -> Statistics {
  auto stats = statistics;

  if (enableVictimCache && victimCache != nullptr) {
    stats.numMiss -= victimCache->statistics.numHit;
    stats.numHit += victimCache->statistics.numHit;
  }

  return stats;
}

Cache::~Cache() { delete victimCache; }

void Cache::setVictimCache(const bool enable) {
  enableVictimCache = enable;
  if (enableVictimCache) {
    constexpr auto victimCachePolicy = Cache::Policy{
        .cacheSize = 8 * 1024,  // 8KB
        .blockSize = 64,
        .blockNum = 8 * 1024 / 64,
        .associativity = 8 * 1024 / 64,
        .hitLatency = 1,
        .missLatency = 8,
    };
    victimCache = new Cache(memoryManager, victimCachePolicy, lowerCache);
  } else {
    victimCache = nullptr;
  }
}

auto Cache::inCache(const uint32_t addr) -> bool {
  return getBlockId(addr) != -1;
}

void Cache::fetch(const uint32_t addr) {
  if (const auto blockId = getBlockId(addr); blockId == -1) {
    loadBlockFromLowerLevel(addr, true);
  }
}

auto Cache::getBlockId(const uint32_t addr) -> uint32_t {
  const auto tag = getTag(addr);
  const auto idx = getId(addr);

  for (auto i = idx * policy.associativity;
       i < (idx + 1) * policy.associativity; ++i) {
    if (blocks[i].id != idx) {
      throw std::runtime_error(std::format("Inconsistent ID in block {}", i));
    }

    if (blocks[i].valid && blocks[i].tag == tag) {
      return i;
    }
  }

  return -1;
}

auto Cache::getByte(const uint32_t addr) -> uint8_t {
  ++referenceCounter;

  auto blockId = getBlockId(addr);
  if (blockId != -1) {  // Hit

    const auto offset = getOffset(addr);
    blocks[blockId].lastReference = referenceCounter;

    return blocks[blockId].data[offset];
  }

  // Miss
  loadBlockFromLowerLevel(addr, true);

  // The block is in the top level cache now, return directly
  blockId = getBlockId(addr);
  if (blockId != -1) {
    const auto offset = getOffset(addr);
    blocks[blockId].lastReference = referenceCounter;
    return blocks[blockId].data[offset];
  }
  throw std::runtime_error(
      std::format("Error: data not in top level cache! addr 0x{:x}", addr));
}

void Cache::setByte(const uint32_t addr, const uint8_t val) {
  ++referenceCounter;

  auto blockId = getBlockId(addr);
  if (blockId != -1) {  // Hit
    blocks[blockId].modified = true;
    blocks[blockId].lastReference = referenceCounter;

    const auto offset = getOffset(addr);
    blocks[blockId].data[offset] = val;

    return;
  }

  // Miss
  loadBlockFromLowerLevel(addr, false);

  blockId = getBlockId(addr);
  if (blockId != -1) {
    blocks[blockId].modified = true;
    blocks[blockId].lastReference = referenceCounter;

    const auto offset = getOffset(addr);
    blocks[blockId].data[offset] = val;
  } else {
    throw std::runtime_error(
        std::format("Error: data not in top level cache!"));
  }
}

void Cache::printInfo(const bool verbose) const {
  std::cout << std::format("---------- Cache Info -----------\n");
  std::cout << std::format("Cache Size: {} bytes\n", policy.cacheSize);
  std::cout << std::format("Block Size: {} bytes\n", policy.blockSize);
  std::cout << std::format("Block Num: {}\n", policy.blockNum);
  std::cout << std::format("Associativity: {}\n", policy.associativity);
  std::cout << std::format("Hit Latency: {}\n", policy.hitLatency);
  std::cout << std::format("Miss Latency: {}\n", policy.missLatency);

  if (verbose) {
    for (std::size_t index = 0; index < blocks.size(); ++index) {
      const auto &block = blocks[index];
      std::cout << std::format(
          "Block {}: tag 0x{:x} id {} {} {} (last ref {})\n", index, block.tag,
          block.id, block.valid ? "valid" : "invalid",
          block.modified ? "modified" : "unmodified", block.lastReference);
    }
  }
}

void Cache::printStatistics() const {
  const auto &[numRead, numWrite, numHit, numMiss, totalCycles] =
      getStatistics();
  std::cout << std::format("-------- STATISTICS ----------\n");
  std::cout << std::format("Num Read: {}\n", numRead);
  std::cout << std::format("Num Write: {}\n", numWrite);
  std::cout << std::format("Num Hit: {}\n", numHit);
  std::cout << std::format("Num Miss: {}\n", numMiss);

  const auto totalAccess = numHit + numMiss;
  const auto missRate = totalAccess > 0 ? (100.F * numMiss / totalAccess) : 0.F;
  std::cout << std::format("Miss Rate: {:.2f}%\n", missRate);

  std::cout << std::format("Total Cycles: {}\n", totalCycles);

  if (enableVictimCache) {
    std::cout << std::format("\n----- VICTIM CACHE -----\n");
    victimCache->printStatistics();
    return;
  }

  if (lowerCache != nullptr) {
    std::cout << std::format("\n---------- LOWER CACHE ----------\n");
    lowerCache->printStatistics();
    std::cout << std::format("\n");
  }
}

auto Cache::isPolicyValid() -> bool {
  const std::array<std::pair<bool, std::string>, 5> checks = {
      {{isPowerOfTwo(policy.cacheSize),
        std::format("Invalid Cache Size {}", policy.cacheSize)},
       {isPowerOfTwo(policy.blockSize),
        std::format("Invalid Block Size {}", policy.blockSize)},
       {policy.cacheSize % policy.blockSize == 0, "cacheSize % blockSize != 0"},
       {policy.blockNum * policy.blockSize == policy.cacheSize,
        "blockNum * blockSize != cacheSize"},
       {policy.blockNum % policy.associativity == 0,
        "blockNum % associativity != 0"}}};

  for (const auto &[condition, message] : checks) {
    if (!condition) {
      std::cerr << std::format("{}\n", message);
      return false;
    }
  }
  return true;
}

void Cache::loadBlockFromLowerLevel(const uint32_t addr, const bool isRead) {
  const auto blockSize = policy.blockSize;

  // Initialize a new block from memory
  Block newBlock = {.valid = true,
                    .modified = false,
                    .tag = getTag(addr),
                    .id = getId(addr),
                    .size = blockSize,
                    .lastReference = referenceCounter,
                    .createdAt = referenceCounter,
                    .data = std::vector<uint8_t>(blockSize)};

  const auto bits = log2i(blockSize);
  const uint32_t mask = ~((1U << bits) - 1);
  const uint32_t blockAddrBegin = addr & mask;

  // Find a block to be replaced
  const auto idx = getId(addr);
  const uint32_t blockIdBegin = idx * policy.associativity;
  const uint32_t blockIdEnd = (idx + 1) * policy.associativity;
  const uint32_t replacedBlockIdx =
      getReplacementBlockId(blockIdBegin, blockIdEnd);
  Block replaceBlock = blocks[replacedBlockIdx];

  bool readFromVictimCache = false;
  if (enableVictimCache) {
    if (isRead) {
      ++victimCache->statistics.numRead;
    } else {
      ++victimCache->statistics.numWrite;
    }
    if (victimCache->inCache(blockAddrBegin)) {
      ++victimCache->statistics.numHit;
      victimCache->statistics.totalCycles += victimCache->policy.hitLatency;
      readFromVictimCache = true;
      for (uint32_t i = blockAddrBegin; i < blockAddrBegin + blockSize; ++i) {
        newBlock.data[i - blockAddrBegin] = victimCache->getByte(i);
      }
      victimCache->setInvalid(blockAddrBegin);
    } else {
      ++victimCache->statistics.numMiss;
      victimCache->statistics.totalCycles += victimCache->policy.missLatency;
    }
  }

  if (!readFromVictimCache) {
    if (lowerCache != nullptr) {
      if (isRead) {
        ++lowerCache->statistics.numRead;
      } else {
        ++lowerCache->statistics.numWrite;
      }
      if (lowerCache->inCache(blockAddrBegin)) {
        ++lowerCache->statistics.numHit;
        lowerCache->statistics.totalCycles += lowerCache->policy.hitLatency;
      } else {
        ++lowerCache->statistics.numMiss;
        lowerCache->statistics.totalCycles += lowerCache->policy.missLatency;
        lowerCache->loadBlockFromLowerLevel(blockAddrBegin, isRead);
      }

      for (uint32_t i = blockAddrBegin; i < blockAddrBegin + blockSize; ++i) {
        newBlock.data[i - blockAddrBegin] = lowerCache->getByte(i);
      }
    } else {
      for (uint32_t i = blockAddrBegin; i < blockAddrBegin + blockSize; ++i) {
        newBlock.data[i - blockAddrBegin] = memoryManager->getByte(i);
      }
    }
  }

  if (replaceBlock.valid) {
    if (replaceBlock.modified) {
      writeBlockToLowerLevel(replaceBlock);
      statistics.totalCycles += policy.missLatency;
    } else if (enableVictimCache && victimCache != nullptr) {
      // Add evicted clean blocks to victim cache too
      const auto evictedAddr = getAddr(replaceBlock);
      for (uint32_t i = 0; i < replaceBlock.size; ++i) {
        victimCache->setByte(evictedAddr + i, replaceBlock.data[i]);
      }
    }
  }

  blocks[replacedBlockIdx] = newBlock;
}

void Cache::setInvalid(const uint32_t addr) {
  if (const auto blockId = getBlockId(addr); blockId != -1) {
    blocks[blockId].valid = false;
  }
}

auto Cache::getReplacementBlockId(const uint32_t begin,
                                  const uint32_t end) const -> uint32_t {
  // Find the first invalid block
  for (auto i = begin; i < end; ++i) {
    if (!blocks[i].valid) {
      return i;
    }
  }

  if (enableFifo) {
    uint32_t blockId = begin;
    uint32_t minCreatedAt = blocks[begin].createdAt;

    for (auto i = begin + 1; i < end; ++i) {
      if (blocks[i].createdAt < minCreatedAt) {
        blockId = i;
        minCreatedAt = blocks[i].createdAt;
      }
    }
    return blockId;
  }
  // Otherwise, find the LRU block
  uint32_t resultId = begin;
  uint32_t min = this->blocks[begin].lastReference;
  for (uint32_t i = begin; i < end; ++i) {
    if (this->blocks[i].lastReference < min) {
      resultId = i;
      min = this->blocks[i].lastReference;
    }
  }
  return resultId;
}

void Cache::writeBlockToLowerLevel(Block &block) {
  const auto addrBegin = getAddr(block);
  if (enableVictimCache && victimCache != nullptr) {
    // ++victimCache->statistics.numWrite;
    for (uint32_t i = 0; i < block.size; ++i) {
      victimCache->setByte(addrBegin + i, block.data[i]);
    }

  } else {
    if (lowerCache != nullptr) {
      // ++lowerCache->statistics.numWrite;
      for (uint32_t i = 0; i < block.size; ++i) {
        lowerCache->setByte(addrBegin + i, block.data[i]);
      }
    } else {
      for (uint32_t i = 0; i < block.size; ++i) {
        memoryManager->setByte(addrBegin + i, block.data[i]);
      }
    }
  }
}

auto Cache::isPowerOfTwo(const uint32_t n) -> bool {
  return n > 0 && (n & (n - 1)) == 0;
}

auto Cache::log2i(const uint32_t val) -> uint32_t {
  return val == 0 ? static_cast<uint32_t>(-1) : std::bit_width(val) - 1;
}

auto Cache::getTag(const uint32_t addr) -> uint32_t {
  const auto offsetBits = log2i(policy.blockSize);
  const auto idBits = log2i(policy.blockNum / policy.associativity);
  const uint32_t mask = (1U << (32 - offsetBits - idBits)) - 1;
  return (addr >> (offsetBits + idBits)) & mask;
}

uint32_t Cache::getId(uint32_t addr) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  uint32_t mask = (1 << idBits) - 1;
  return (addr >> offsetBits) & mask;
}

uint32_t Cache::getOffset(uint32_t addr) {
  uint32_t bits = log2i(policy.blockSize);
  uint32_t mask = (1 << bits) - 1;
  return addr & mask;
}

auto Cache::getAddr(const Block &block) const -> uint32_t {
  const uint32_t offsetBits = log2i(policy.blockSize);
  const uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  return (block.tag << (offsetBits + idBits)) | (block.id << offsetBits);
}

auto Cache::read(const uint32_t addr) -> uint8_t {
  ++statistics.numRead;

  if (inCache(addr)) {
    ++statistics.numHit;
    statistics.totalCycles += policy.hitLatency;
  } else {
    ++statistics.numMiss;
    statistics.totalCycles += policy.missLatency;
  }

  return getByte(addr);
}

void Cache::write(const uint32_t addr, const uint8_t val) {
  ++statistics.numWrite;

  if (inCache(addr)) {
    ++statistics.numHit;
    statistics.totalCycles += policy.hitLatency;
  } else {
    ++statistics.numMiss;
    statistics.totalCycles += policy.missLatency;
  }
  setByte(addr, val);
}
