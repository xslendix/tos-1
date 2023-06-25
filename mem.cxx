#include "mem.hxx"

#include <fstream>
#include <ios>
using std::ios;
#include <filesystem>
namespace fs = std::filesystem;
#include <iostream>
#include <memory>
#include <utility>

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

static int64_t Hex2I64(char const* ptr, char const** res) {
  int64_t ret = 0;
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
    ret = mmap(nullptr, sz / ps * ps + pad, PROT_EXEC | PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON | MAP_32BIT, -1, 0);
#ifdef __linux__
    // I hear that linux doesn't like addresses within the first 16bits
    if (ret == MAP_FAILED) {
      auto down = reinterpret_cast<char*>((uintptr_t)0x10000);
      std::ifstream map{"/proc/self/maps", ios::binary | ios::in};
      std::string s, buffer;
      while (std::getline(map, buffer))
        (s += buffer) += '\n';
      int64_t len;
      const char* ptr = s.c_str();
      while (true) {
        auto lower = (char*)Hex2I64(ptr, &ptr);
        if ((lower - down) >= (sz / ps * ps + pad) && lower > down) {
          goto found;
        }
        // Ignore '-'
        ++ptr;
        auto upper = (char*)Hex2I64(ptr, &ptr);
        down = upper;
        ptr = strchr(ptr, '\n');
        if (ptr == nullptr) // end of file but still not found
          break;
        ++ptr; // go to next line(ptr is at '\n')
      }
    found:
      ret = mmap(down, sz / ps * ps + pad, PROT_EXEC | PROT_WRITE | PROT_READ,
                 MAP_PRIVATE | MAP_ANON | MAP_FIXED | MAP_32BIT, -1, 0);
    }
#endif
  } else // data heap
    ret = mmap(nullptr, sz / ps * ps + pad, PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON, -1, 0);
  if (ret == MAP_FAILED)
    return NULL;
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
    while (alloc <= 0xFFffFFff) {
      if (!VirtualQuery((void*)alloc, &ent, sizeof(ent)))
        return nullptr;
      alloc = (uint64_t)ent.BaseAddress + ent.RegionSize;
      // Fancy code to round up because
      // address is rounded down with
      // VirtualAlloc
      addr = ((uint64_t)ent.BaseAddress + dwAllocationGranularity - 1) &
             ~(dwAllocationGranularity - 1);
      if ((ent.State == MEM_FREE) && (sz <= (alloc - addr))) {
        return VirtualAlloc((void*)addr, sz, MEM_COMMIT | MEM_RESERVE,
                            PAGE_EXECUTE_READWRITE);
      }
    }
    std::cerr << "Out of 32bit virtual address space\n";
    std::terminate();
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
