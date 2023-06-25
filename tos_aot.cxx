#include "tos_aot.hxx"
#include "dbg.hxx"
#include "ffi.h"
#include "mem.hxx"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;
#include <ios>
#include <iostream>
using std::ios;
#include <fstream>
#include <string>
#include <utility>

MapCHashVec TOSLoader;

// This code looks like utter garbage and it is. Sorry for that
static void LoadOneImport(char** src_, char* mod_base) {
  char *src = *src_, *st_ptr, *ptr = nullptr;
  uint64_t i, etype;
  char first = 1;
  while ((etype = *src++)) {
    ptr = mod_base + *(int32_t*)src;
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    // First occurance of a string means "repeat this until another name is
    // found"
    if (*st_ptr) {
      if (!first) {
        *src_ = st_ptr - 5;
        return;
      } else {
        first = 0;
        if (TOSLoader.find(st_ptr) == TOSLoader.end()) {
          std::cerr << "Unresolved reference " << st_ptr << std::endl;
          CHash tmpiss;
          tmpiss.type = HTT_IMPORT_SYS_SYM;
          tmpiss.mod_header_entry = st_ptr - 5;
          tmpiss.mod_base = mod_base;
          TOSLoader[st_ptr].emplace_back(tmpiss);
        } else {
          auto& v = TOSLoader[st_ptr];
          for (auto& tmp : v) {
            if (tmp.type == HTT_IMPORT_SYS_SYM)
              continue;
            i = (int64_t)tmp.val;
            break;
          }
        }
      }
    }
    switch (etype) {
    case IET_REL_I8:
      *ptr = (char*)i - ptr - 1;
      break;
    case IET_IMM_U8:
      *ptr = (uintptr_t)i;
      break;
    case IET_REL_I16:
      *(int16_t*)ptr = (char*)i - ptr - 2;
      break;
    case IET_IMM_U16:
      *(int16_t*)ptr = (int64_t)i;
      break;
    case IET_REL_I32:
      *(int32_t*)ptr = (char*)i - ptr - 4;
      break;
    case IET_IMM_U32:
      *(int32_t*)ptr = (int64_t)i;
      break;
    case IET_REL_I64:
      *(int64_t*)ptr = (char*)i - ptr - 8;
      break;
    case IET_IMM_I64:
      *(int64_t*)ptr = (int64_t)i;
      break;
    }
  }
  *src_ = src - 1;
}

static void SysSymImportsResolve(char* st_ptr) {
  char* ptr;
  for (auto& tmp : TOSLoader) {
    auto& syms = std::get<std::vector<CHash>>(tmp);
    for (auto& sym : syms)
      if (sym.type == HTT_IMPORT_SYS_SYM) {
        ptr = sym.mod_header_entry;
        LoadOneImport(&ptr, sym.mod_base);
        sym.type = HTT_INVALID;
      }
  }
}

static void LoadPass1(char* src, char* mod_base) {
  char *ptr, *st_ptr;
  int64_t i, cnt, etype;
  CHash tmpex;
  while ((etype = *src++)) {
    i = *((int32_t*)src);
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_REL32_EXPORT:
    case IET_IMM32_EXPORT:
    case IET_REL64_EXPORT:
    case IET_IMM64_EXPORT:
      tmpex.type = HTT_EXPORT_SYS_SYM;
      if (etype == IET_IMM32_EXPORT || etype == IET_IMM64_EXPORT)
        tmpex.val = (void*)i;
      else
        tmpex.val = i + mod_base;
      TOSLoader[st_ptr].emplace_back(tmpex);
      SysSymImportsResolve(st_ptr);
      break;
    case IET_REL_I0 ... IET_IMM_I64:
      src = st_ptr - 5;
      LoadOneImport(&src, mod_base);
      break;
    case IET_ABS_ADDR: {
      cnt = i;
      for (int64_t j = 0; j < cnt; j++) {
        ptr = mod_base + *(int32_t*)src;
        src += 4;
        *(int32_t*)ptr += (uintptr_t)mod_base;
      }
    } break;
    default:;
    }
  }
}

static void LoadPass2(char* src, char* mod_base) {
  char* st_ptr;
  int64_t i, etype;
  while ((etype = *src++)) {
    i = *(int32_t*)src;
    src += 4;
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_MAIN:
      FFI_CALL_TOS_0_ZERO_BP(mod_base + i);
      break;
    case IET_ABS_ADDR:
      src += sizeof(int32_t) * i;
      break;
    case IET_CODE_HEAP:
    case IET_ZEROED_CODE_HEAP:
      src += 4 + sizeof(int32_t) * i;
      break;
    case IET_DATA_HEAP:
    case IET_ZEROED_DATA_HEAP:
      src += 8 + sizeof(int32_t) * i;
      break;
    }
  }
}

extern "C" struct __attribute__((packed)) CBinFile {
  uint16_t jmp;
  uint8_t mod_align_bits, pad;
  union {
    char bin_signature[4];
    uint32_t sig;
  };
  int64_t org, patch_table_offset, file_size;
};

void LoadHCRT(std::string const& name) {
  std::ifstream f{name, ios::in | ios::binary};
  if (!f) {
    std::cerr << "CANNOT FIND TEMPLEOS BINARY FILE " << name << std::endl;
    std::terminate();
  }
  char* mod_base;
  CBinFile *bfh, *bfh_addr;
  size_t size = fs::file_size(name);
  f.read(reinterpret_cast<char*>(bfh_addr = bfh =
                                     (CBinFile*)NewVirtualChunk(size, true)),
         size);
  if (memcmp(bfh->bin_signature, "TOSB", 4) != 0) {
    std::cerr << "INVALID TEMPLEOS BINARY FILE " << name << std::endl;
    std::terminate();
  }
  mod_base = (char*)bfh_addr + sizeof(CBinFile);
  LoadPass1((char*)bfh_addr + bfh_addr->patch_table_offset, mod_base);
#ifndef _WIN32
  // signal(SIGUSR2, (void (*)(int))TOSLoader["__InterruptCoreRoutine"][0].val);
#endif
  SetupDebugger();
  LoadPass2((char*)bfh_addr + bfh_addr->patch_table_offset, mod_base);
}

__attribute__((noinline)) void BackTrace() {
  static size_t sz = 0;
  std::string last;
  static std::vector<std::string> sorted;
  static bool init = false;
  if (!init) {
    for (auto const& e : TOSLoader) {
      auto const& [name, v] = e;
      if (v.size() > 0)
        sorted.emplace_back(name);
    }
    sz = sorted.size();
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
      return TOSLoader[a][0].val < TOSLoader[b][0].val;
    });
    init = true;
  }
  auto rbp = (void**)__builtin_frame_address(0);
  void* oldp;
  void* ptr = __builtin_return_address(1);
  while (rbp) {
    oldp = nullptr;
    last = "UNKOWN";
    size_t idx;
    for (idx = 0; idx < sz; idx++) {
      void* curp = TOSLoader[sorted[idx]][0].val;
      if (curp == ptr) {
        std::cerr << sorted[idx] << std::endl;
      } else if (curp > ptr) {
        std::cerr << last << "[" << ptr << "+"
                  << reinterpret_cast<void*>((char*)ptr - (char*)oldp) << "]\n";
        goto next;
      }
      oldp = curp;
      last = sorted[idx];
    }
  next:;
    ptr = rbp[1];
    rbp = (void**)*rbp;
  }
}

std::string WhichFun(void* ptr) {
  size_t sz = TOSLoader.size();
  std::string last;
  static std::vector<std::string> sorted;
  static bool init = false;
  if (!init) {
    for (auto const& e : TOSLoader) {
      auto const& [name, v] = e;
      if (v.size() > 0)
        sorted.emplace_back(name);
    }
    std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
      return TOSLoader[a][0].val < TOSLoader[b][0].val;
    });
    init = true;
  }
  for (size_t idx = 0; idx < sz; idx++) {
    void* curp = TOSLoader[sorted[idx]][0].val;
    if (curp == ptr) {
      std::cerr << sorted[idx] << std::endl;
    } else if (curp > ptr) {
      return last;
    }
    last = sorted[idx];
  }
  std::cout << last << std::endl;
  return last;
}
