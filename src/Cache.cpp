#include "Cache.h"

#include <array>
#include <bit>
#include <format>
#include <iostream>
#include <print>
#include <ranges>

Cache::Cache(std::shared_ptr<MemoryManager> manager, const Policy &policy,
             std::unique_ptr<Cache> lowerCache)
    : statistics({.numRead = 0,
                  .numWrite = 0,
                  .numHit = 0,
                  .numMiss = 0,
                  .totalCycles = 0}),
      referenceCounter(0),
      memoryManager(std::move(manager)),
      lowerCache(std::move(lowerCache)),
      policy(policy) {
  if (!isPolicyValid()) {
    throw std::runtime_error("Invalid cache policy");
  }

  blocks.resize(policy.blockNum);
  for (const auto idx :
       std::views::iota(0U, static_cast<uint32_t>(blocks.size()))) {
    auto &[valid, modified, tag, id, size, lastReference, data] = blocks[idx];
    valid = false;
    modified = false;
    tag = 0;
    id = idx / policy.associativity;
    size = policy.blockSize;
    lastReference = 0;
    data = std::vector<uint8_t>(size);
  }
}

auto Cache::inCache(const uint32_t addr) -> bool {
  return getBlockId(addr) != -1;
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
  ++statistics.numRead;

  auto blockId = getBlockId(addr);
  if (blockId != -1) {  // Hit
    ++statistics.numHit;

    const auto offset = getOffset(addr);
    statistics.totalCycles += policy.hitLatency;
    blocks[blockId].lastReference = referenceCounter;

    return blocks[blockId].data[offset];
  }

  // Miss
  ++statistics.numMiss;
  statistics.totalCycles += policy.missLatency;
  loadBlockFromLowerLevel(addr);

  // The block is in top level cache now, return directly
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
  ++statistics.numWrite;

  auto blockId = getBlockId(addr);
  if (blockId != -1) {  // Hit

    statistics.numHit++;
    statistics.totalCycles += policy.hitLatency;
    blocks[blockId].modified = true;
    blocks[blockId].lastReference = referenceCounter;

    const auto offset = getOffset(addr);
    blocks[blockId].data[offset] = val;

    return;
  }

  // Miss
  ++statistics.numMiss;
  statistics.totalCycles += policy.missLatency;

  loadBlockFromLowerLevel(addr);

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
  std::print("---------- Cache Info -----------\n");
  std::print("Cache Size: {} bytes\n", policy.cacheSize);
  std::print("Block Size: {} bytes\n", policy.blockSize);
  std::print("Block Num: {}\n", policy.blockNum);
  std::print("Associativity: {}\n", policy.associativity);
  std::print("Hit Latency: {}\n", policy.hitLatency);
  std::print("Miss Latency: {}\n", policy.missLatency);

  if (verbose) {
    for (std::size_t index = 0; index < blocks.size(); ++index) {
      const auto &block = blocks[index];
      std::print("Block {}: tag 0x{:x} id {} {} {} (last ref {})\n", index,
                 block.tag, block.id, block.valid ? "valid" : "invalid",
                 block.modified ? "modified" : "unmodified",
                 block.lastReference);
    }
  }
}

void Cache::printStatistics() const {
  std::print("-------- STATISTICS ----------\n");
  std::print("Num Read: {}\n", statistics.numRead);
  std::print("Num Write: {}\n", statistics.numWrite);
  std::print("Num Hit: {}\n", statistics.numHit);
  std::print("Num Miss: {}\n", statistics.numMiss);

  const auto totalAccess = statistics.numHit + statistics.numMiss;
  const auto missRate =
      totalAccess > 0 ? (100.F * statistics.numMiss / totalAccess) : 0.F;
  std::print("Miss Rate: {:.2f}%\n", missRate);

  std::print("Total Cycles: {}\n", statistics.totalCycles);

  if (lowerCache) {
    std::print("---------- LOWER CACHE ----------\n");
    lowerCache->printStatistics();
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
      std::print(std::cerr, "{}\n", message);
      return false;
    }
  }
  return true;
}

void Cache::loadBlockFromLowerLevel(const uint32_t addr) {
  const auto blockSize = policy.blockSize;

  // Initialize new block from memory
  Block block = {.valid = true,
                 .modified = false,
                 .tag = getTag(addr),
                 .id = getId(addr),
                 .size = blockSize,
                 .lastReference = referenceCounter,
                 .data = std::vector<uint8_t>(blockSize)};

  const auto bits = log2i(blockSize);
  const uint32_t mask = ~((1U << bits) - 1);
  const uint32_t blockAddrBegin = addr & mask;

  if (lowerCache) {
    for (uint32_t i = blockAddrBegin; i < blockAddrBegin + blockSize; ++i) {
      block.data[i - blockAddrBegin] = lowerCache->getByte(i);
    }
  } else {
    for (uint32_t i = blockAddrBegin; i < blockAddrBegin + blockSize; ++i) {
      block.data[i - blockAddrBegin] = memoryManager->getByte(i);
    }
  }

  // Find block to be replaced
  const auto idx = getId(addr);
  const uint32_t blockIdBegin = idx * policy.associativity;
  const uint32_t blockIdEnd = (idx + 1) * policy.associativity;
  const uint32_t replacedBlockIdx =
      getReplacementBlockId(blockIdBegin, blockIdEnd);

  if (Block replaceBlock = blocks[replacedBlockIdx];
      replaceBlock.valid && replaceBlock.modified) {
    writeBlockToLowerLevel(replaceBlock);
    statistics.totalCycles += policy.missLatency;
  }

  blocks[replacedBlockIdx] = block;
}

auto Cache::getReplacementBlockId(const uint32_t begin,
                                  const uint32_t end) const -> uint32_t {
  // Find the first invalid block
  for (auto i = begin; i < end; ++i) {
    if (!blocks[i].valid) {
      return i;
    }
  }

  // Otherwise, find the LRU block
  const auto lruIt = std::ranges::min_element(
      blocks | std::views::drop(begin) | std::views::take(end - begin),
      [](const Block &a, const Block &b) {
        return a.lastReference < b.lastReference;
      });
  return std::distance(blocks.begin(), lruIt);
}

void Cache::writeBlockToLowerLevel(Block &block) {
  const auto addrBegin = getAddr(block);

  if (lowerCache) {
    std::ranges::for_each(std::views::iota(0U, block.size),
                          [&](const uint32_t i) {
                            lowerCache->setByte(addrBegin + i, block.data[i]);
                          });

  } else {
    std::ranges::for_each(
        std::views::iota(0U, block.size), [&](const uint32_t i) {
          memoryManager->setByte(addrBegin + i, block.data[i]);
        });
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

uint32_t Cache::getAddr(Cache::Block &block) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  return (block.tag << (offsetBits + idBits)) | (block.id << offsetBits);
}

auto Cache::read(const uint32_t addr) -> uint8_t { return getByte(addr); }

void Cache::write(const uint32_t addr, const uint8_t val) {
  setByte(addr, val);
}
