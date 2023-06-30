#include "mem.hxx"

#include <fstream>
#include <ios>
using std::ios;
#include <memory>
#include <utility>

#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <memoryapi.h>
#include <sysinfoapi.h>
#include <winnt.h>
#endif

static inline uint64_t Hex2U64(char const* ptr, char const** res) {
  uint64_t ret = 0;
  char c;
  while (isxdigit(c = *ptr)) {
    ret <<= 4;
    ret |= isalpha(c) ? toupper(c) - 'A' + 10 : c - '0';
    ++ptr;
  }
  if (res)
    *res = ptr;
  return ret;
}

void* NewVirtualChunk(size_t sz, bool low32) {
#ifndef _WIN32
  static size_t ps = 0;
  if (!ps)
    ps = sysconf(_SC_PAGE_SIZE);
  size_t pad = ps;
  void* ret;
  pad = sz % ps;
  if (pad)
    pad = ps;
  if (low32) { // code heap
    // sz / ps * ps seems meaningless
    // but its actually aligning sz to ps(page size)
    ret = mmap(nullptr, sz / ps * ps + pad, PROT_EXEC | PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON | MAP_32BIT, -1, 0);
#ifdef __linux__
    // I hear that linux doesn't like addresses within the first 16bits
    // maybe its the stack space?
    if (ret == MAP_FAILED) {
      uintptr_t down = 0x1000;
      std::ifstream map{"/proc/self/maps", ios::binary | ios::in};
      std::string buffer;
      // just fs::file_size() wont work lmao
      while (std::getline(map, buffer)) {
        char const* ptr = buffer.data();
        uint64_t lower = Hex2U64(ptr, &ptr);
        // basically finds a gap between the previous line's upper address
        // and the current line's lower address so it can allocate there
        if (lower - down >= sz / ps * ps + pad && lower > down) {
          goto found;
        }
        // ignore '-'
        ++ptr;
        uint64_t upper = Hex2U64(ptr, &ptr);
        down = upper;
      }
    found:
      if (down > UINT32_MAX)
        return nullptr;
      ret = mmap(reinterpret_cast<void*>(down), sz / ps * ps + pad,
                 PROT_EXEC | PROT_WRITE | PROT_READ,
                 MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    } else
      return ret;
#endif
  } else // data heap
    ret = mmap(nullptr, sz / ps * ps + pad, PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON, -1, 0);
  if (ret == MAP_FAILED)
    return nullptr;
  return ret;
#else
  if (low32) { // code heap
    // https://archive.md/ugIUC
    static DWORD dwAllocationGranularity = 0;
    if (dwAllocationGranularity == 0) {
      SYSTEM_INFO si;
      GetSystemInfo(&si);
      dwAllocationGranularity = si.dwAllocationGranularity;
    }
    MEMORY_BASIC_INFORMATION ent;
    uint64_t alloc = dwAllocationGranularity, addr;
    while (alloc <= UINT32_MAX) {
      if (!VirtualQuery((void*)alloc, &ent, sizeof(ent)))
        return nullptr;
      alloc = (uint64_t)ent.BaseAddress + ent.RegionSize;
      // Fancy code to round up because
      // address is rounded down with
      // VirtualAlloc
      addr = ((uint64_t)ent.BaseAddress + dwAllocationGranularity - 1) &
             ~(dwAllocationGranularity - 1);
      if (ent.State == MEM_FREE && sz <= alloc - addr)
        return VirtualAlloc((void*)addr, sz, MEM_COMMIT | MEM_RESERVE,
                            PAGE_EXECUTE_READWRITE);
    }
    return nullptr;
  } else // data heap
    return VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
}

void FreeVirtualChunk(void* ptr, size_t s) {
#ifdef _WIN32
  VirtualFree(ptr, 0, MEM_RELEASE);
#else
  static size_t ps = 0;
  if (!ps)
    ps = sysconf(_SC_PAGE_SIZE);
  int64_t pad;
  pad = s % ps;
  if (pad)
    pad = ps;
  munmap(ptr, s / ps * ps + pad);
#endif
}
