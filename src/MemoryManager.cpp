#include "MemoryManager.h"

#include <cstdio>
#include <string>

#include "Debug.h"

MemoryManager::MemoryManager() {
  for (uint32_t i = 0; i < 1024; ++i) {
    this->memory[i] = nullptr;
  }
}

MemoryManager::~MemoryManager() {
  for (uint32_t i = 0; i < 1024; ++i) {
    if (this->memory[i] != nullptr) {
      for (uint32_t j = 0; j < 1024; ++j) {
        if (this->memory[i][j] != nullptr) {
          delete[] this->memory[i][j];
          this->memory[i][j] = nullptr;
        }
      }
      delete[] this->memory[i];
      this->memory[i] = nullptr;
    }
  }
}

bool MemoryManager::addPage(uint32_t addr) {
  uint32_t i = this->getFirstEntryId(addr);
  uint32_t j = this->getSecondEntryId(addr);
  if (this->memory[i] == nullptr) {
    this->memory[i] = new uint8_t *[1024];
    memset(this->memory[i], 0, sizeof(uint8_t *) * 1024);
  }
  if (this->memory[i][j] == nullptr) {
    this->memory[i][j] = new uint8_t[4096];
    memset(this->memory[i][j], 0, 4096);
  } else {
    dbgprintf("Addr 0x%x already exists and do not need an addPage()!\n", addr);
    return false;
  }
  return true;
}

bool MemoryManager::isPageExist(uint32_t addr) {
  return this->isAddrExist(addr);
}

bool MemoryManager::setByte(uint32_t addr, uint8_t val, uint32_t *cycles) {
  if (!this->isAddrExist(addr)) {
    dbgprintf("Byte write to invalid addr 0x%x!\n", addr);
    return false;
  }

  uint32_t i = this->getFirstEntryId(addr);
  uint32_t j = this->getSecondEntryId(addr);
  uint32_t k = this->getPageOffset(addr);
  this->memory[i][j][k] = val;
  return true;
}

uint8_t MemoryManager::getByte(uint32_t addr, uint32_t *cycles) {
  if (!this->isAddrExist(addr)) {
    dbgprintf("Byte read to invalid addr 0x%x!\n", addr);
    return false;
  }
  uint32_t i = this->getFirstEntryId(addr);
  uint32_t j = this->getSecondEntryId(addr);
  uint32_t k = this->getPageOffset(addr);
  return this->memory[i][j][k];
}

uint32_t MemoryManager::getFirstEntryId(uint32_t addr) {
  return (addr >> 22) & 0x3FF;
}

uint32_t MemoryManager::getSecondEntryId(uint32_t addr) {
  return (addr >> 12) & 0x3FF;
}

uint32_t MemoryManager::getPageOffset(uint32_t addr) { return addr & 0xFFF; }

bool MemoryManager::isAddrExist(uint32_t addr) {
  uint32_t i = this->getFirstEntryId(addr);
  uint32_t j = this->getSecondEntryId(addr);
  if (memory[i] && memory[i][j]) {
    return true;
  }
  return false;
}
