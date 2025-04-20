/*
 * A Memory Simulator Similar to X86 Virtual Memory
 * Created by He, Hao at 2019-3-11
 */

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstdint>
#include <string>

class MemoryManager {
 public:
  MemoryManager();
  ~MemoryManager();

  bool addPage(uint32_t addr);
  bool isPageExist(uint32_t addr);

  bool setByte(uint32_t addr, uint8_t val, uint32_t *cycles = nullptr);
  uint8_t getByte(uint32_t addr, uint32_t *cycles = nullptr);

 private:
  uint32_t getFirstEntryId(uint32_t addr);
  uint32_t getSecondEntryId(uint32_t addr);
  uint32_t getPageOffset(uint32_t addr);
  bool isAddrExist(uint32_t addr);

  uint8_t **memory[1024];
};

#endif
