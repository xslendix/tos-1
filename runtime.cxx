#include "runtime.hxx"
#include "TOSPrint.hxx"
#include "ffi.h"
#include "main.hxx"
#include "mem.hxx"
#include "multic.hxx"
#ifndef HEADLESS
#include "sdl_window.hxx"
#include "sound.h"
#endif
#include "tos_aot.hxx"
#include "vfs.hxx"

#include <ios>
#include <string>
#include <vector>
using std::ios;
#include <filesystem>
#include <fstream>
#include <memory>
namespace fs = std::filesystem;
#include <thread>
using std::thread;

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <winbase.h>
#include <fileapi.h>
#include <memoryapi.h>
#include <shlwapi.h>
// clang-format on
#else
#include "ext/linenoise.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
// todo restore __GetUVLoopMode

void HolyFree(void* ptr) {
  static void* fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_FREE"][0].val;
  FFI_CALL_TOS_1(fptr, (uintptr_t)ptr);
}

void* HolyMAlloc(size_t sz) {
  static void* fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_MALLOC"][0].val;
  return (void*)FFI_CALL_TOS_2(fptr, (uint64_t)sz, (uintptr_t) nullptr);
}

char* HolyStrDup(char const* str) {
  return strcpy((char*)HolyMAlloc(strlen(str) + 1), str);
}

static FILE* VFsFOpen(char const* path, char const* m) {
  std::string p = VFsFileNameAbs(path);
  return fopen(p.c_str(), m);
}

#include "ext/dyad.h"
#include <uv.h>

/*
 * uv_udp_t
 * uv_udp_send_t
 * uv_udp_flags { UV_UDP_IPV6ONLY, UV_UDP_PARTIAL, UV_UDP_REUSEADDR,
 * UV_UDP_MMSG_CHUNK, UV_UDP_MMSG_FREE, UV_UDP_LINUX_RECVERR, UV_UDP_RECVMSG }
 * void (*)uv_udp_send_cb(uv_udp_send_t *req, int status)
 * void (*)uv_udp_recv_cb(uv_udp_t *handle, ssize_t nread, uv_buf_t const *buf,
 *                        struct sockaddr const *addr, unsigned flags)
 *       -> nread 0 ~
 *       -> uv_buf_t -> len  size_t
 *                   -> base char*
 * uv_interface_address_t
 *       -> name char*
 *       -> phys_addr char[6]
 *       -> is_internal bool
 *       -> address sockaddr_in{,6} union
 *       -> netmask ditto ^
 *
 * uv_random(uv_loop_t *loop, uv_random_t *req, void *buf,
 *           size_t buflen, unsigned int flags, uv_random_cb cb)
 *       -> void (*)uv_random_cb(uv_random_t *req, int status,
 *                               void *buf, size_t buflen)
 *
 * req->data <- &TOSFunc;
 *
 * */

static void UVUDPSendCB(uv_udp_t* h, ssize_t nread, uv_buf_t const* buf,
                        struct sockaddr const* addr, unsigned flags) {
}
static void UVRandomCB(uv_random_t* req, int status, void* buf, size_t buflen) {
  FFI_CALL_TOS_4(req->data, (uintptr_t)req, status, (uintptr_t)buf, buflen);
}

static int64_t STK_UVRandom(int64_t* stk) {
  ((uv_random_t*)stk[1])->data = (void*)(uintptr_t)stk[5];
  return uv_random((uv_loop_t*)stk[0], (uv_random_t*)stk[1], (void*)stk[2],
                   (size_t)stk[3], stk[4], &UVRandomCB);
}
static void* STK_UVRandomNew() {
  return new uv_random_t;
}

static void STK_UVRandomDel(int64_t* stk) {
  delete (uv_random_t*)stk[0];
}

static int64_t STK_UVBufLen(int64_t* stk) {
  return ((uv_buf_t*)stk[0])->len;
}

static int64_t STK_UVBufBase(int64_t* stk) {
  return (uintptr_t)((uv_buf_t*)stk[0])->base;
}

static int64_t STK_UVIP4Addr(int64_t* stk) {
  return uv_ip4_addr((char const*)stk[0], (int)stk[1],
                     (struct sockaddr_in*)stk[2]);
}

static int64_t STK_UVIP6Addr(int64_t* stk) {
  return uv_ip6_addr((char const*)stk[0], (int)stk[1],
                     (struct sockaddr_in6*)stk[2]);
}

static int64_t STK_UVIP4Name(int64_t* stk) {
  return uv_ip4_name((struct sockaddr_in const*)stk[0], (char*)stk[1],
                     (size_t)stk[2]);
}

static int64_t STK_UVIP6Name(int64_t* stk) {
  return uv_ip6_name((struct sockaddr_in6 const*)stk[0], (char*)stk[1],
                     (size_t)stk[2]);
}

static int64_t STK_UVIPName(int64_t* stk) {
  return uv_ip_name((struct sockaddr const*)stk[0], (char*)stk[1],
                    (size_t)stk[2]);
}

static void* STK_UVLoopNew() {
  auto ret = new uv_loop_t;
  if (uv_loop_init(ret))
    return NULL;
  return ret;
}

static void STK_UVLoopDel(int64_t* stk) {
  auto l = (uv_loop_t*)stk[0];
  uv_stop(l);
  delete l;
}

static int64_t STK_UVRun(int64_t* stk) {
  return uv_run((uv_loop_t*)stk[0], (uv_run_mode)stk[1]);
}
static void* STK_UVUDPNew(int64_t* stk) {
  auto ret = new uv_udp_t;
  if (uv_udp_init((uv_loop_t*)stk[0], ret))
    return NULL;
  return ret;
}
// 0suc
static void STK_UVUDPRecvStop(int64_t* stk) {
  uv_udp_recv_stop((uv_udp_t*)stk[0]);
}

static void STK_UVUDPDel(int64_t* stk) {
  delete (uv_udp_t*)stk[0];
}

static int64_t STK_UVUDPBind(int64_t* stk) {
  return (uintptr_t)uv_udp_bind(
      (uv_udp_t*)stk[0], (struct sockaddr const*)stk[1], (unsigned)stk[2]);
}

static int64_t STK_UVUDPConnect(int64_t* stk) {
  return (uintptr_t)uv_udp_connect((uv_udp_t*)stk[0],
                                   (struct sockaddr const*)stk[1]);
}

static void STK_DyadInit() {
  static bool init = false;
  if (init)
    return;
  init = true;
  dyad_init();
  dyad_setUpdateTimeout(0.);
}

static void STK_DyadUpdate() {
  dyad_update();
}

static void STK_DyadShutdown() {
  dyad_shutdown();
}

static void* STK_DyadNewStream() {
  return dyad_newStream();
}

static int64_t STK_DyadListen(int64_t* stk) {
  return dyad_listen((dyad_Stream*)stk[0], stk[1]);
}

static int64_t STK_DyadConnect(int64_t* stk) {
  return dyad_connect((dyad_Stream*)stk[0], (char const*)stk[1], stk[2]);
}

static void STK_DyadWrite(int64_t* stk) {
  dyad_write((dyad_Stream*)stk[0], (void*)stk[1], (int)stk[2]);
}

static void STK_DyadEnd(int64_t* stk) {
  dyad_end((dyad_Stream*)stk[0]);
}

static void STK_DyadClose(int64_t* stk) {
  dyad_close((dyad_Stream*)stk[0]);
}

static char* STK_DyadGetAddress(int64_t* stk) {
  const char* ret = dyad_getAddress((dyad_Stream*)stk[0]);
  return HolyStrDup(ret);
}

static void DyadReadCB(dyad_Event* e) {
  FFI_CALL_TOS_4(e->udata, (uintptr_t)e->stream, (uintptr_t)e->data, e->size,
                 (uintptr_t)e->udata2);
}

static void STK_DyadSetReadCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_LINE, DyadReadCB,
                   (void*)stk[1], (void*)stk[2]);
}

static void DyadListenCB(dyad_Event* e) {
  FFI_CALL_TOS_2(e->udata, (uintptr_t)e->remote, (uintptr_t)e->udata2);
}

static void DyadCloseCB(dyad_Event* e) {
  FFI_CALL_TOS_2(e->udata, (uintptr_t)e->stream, (uintptr_t)e->udata2);
}
static void STK_DyadSetOnCloseCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_CLOSE, &DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnConnectCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_CONNECT, &DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnDestroyCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_DESTROY, &DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnErrorCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_ERROR, &DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnReadyCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_READY, &DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnTickCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_TICK, &DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetOnTimeoutCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_TIMEOUT, &DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}

static void STK_DyadSetOnListenCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_ACCEPT, &DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}

static void STK_DyadSetTimeout(int64_t* stk) {
  dyad_setTimeout((dyad_Stream*)stk[0], ((double*)stk)[1]);
}

static void STK_DyadSetNoDelay(int64_t* stk) {
  dyad_setNoDelay((dyad_Stream*)stk[0], stk[1]);
}

static void STK_UnblockSignals() {
#ifndef _WIN32
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_UNBLOCK, &all, NULL);
#endif
}

static void STK__GrPaletteColorSet(int64_t* stk) {
#ifndef HEADLESS
  GrPaletteColorSet(stk[0], stk[1]);
#endif
}

static uint64_t STK___IsValidPtr(uintptr_t* stk) {
#ifdef _WIN32
  // Wine doesnt like the
  // IsBadReadPtr,so use a polyfill

  // wtf IsBadReadPtr gives me a segfault so i just have to use this
  // polyfill lmfao
  // #ifdef __WINE__
  MEMORY_BASIC_INFORMATION mbi = {0};
  if (VirtualQuery((void*)stk[0], &mbi, sizeof(mbi))) {
    // https://archive.md/ehBq4
    DWORD mask = (stk[0] <= UINT32_MAX)
                   ? (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                      PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                      PAGE_EXECUTE_WRITECOPY)
                   : (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY);
    return !!(mbi.Protect & mask);
  }
  return 0;
  /*#else
    return !IsBadReadPtr((void*)stk[0], 8);
  #endif*/

#else
  // #ifdef __FreeBSD__
  static size_t ps = 0;
  if (!ps)
    ps = getpagesize();
  stk[0] /= ps; // align to
  stk[0] *= ps; // page boundary
  // https://archive.md/Aj0S4
  return -1 != msync((void*)stk[0], ps, MS_ASYNC);
  /*#elif defined(__linux__)
        // TOO FUCKING GODDAMN SLOW!!!!!
    auto constexpr Hex2U64 = [](char const *ptr, char const** res) {
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
    };
    std::ifstream map{"/proc/self/maps", ios::binary | ios::in};
    std::string buffer;
    while (std::getline(map, buffer)) {
      char const* ptr = buffer.data();
      uintptr_t lower = Hex2U64(ptr, &ptr);
      ++ptr; // skip '-'
      uintptr_t upper = Hex2U64(ptr, &ptr);
      if (lower <= stk[0] && stk[0] <= upper)
        return 1;
    }
    return 0;
  #endif*/

#endif
}

static void STK_InterruptCore(int64_t* stk) {
  InterruptCore(stk[0]);
}

static void STK___BootstrapForeachSymbol(uintptr_t* stk) {
  for (auto& m : TOSLoader) {
    auto& [symname, vec] = m;
    if (vec.size() == 0)
      continue;
    auto& sym = vec[0];
    FFI_CALL_TOS_3((void*)stk[0], (uintptr_t)symname.c_str(),
                   (uintptr_t)sym.val,
                   sym.type == HTT_EXPORT_SYS_SYM ? HTT_FUN : sym.type);
  }
}

static void STK_TOSPrint(uint64_t* stk) {
  TOSPrint((char const*)stk[0], stk[1], (int64_t*)stk + 2);
}

static int64_t STK_IsDir(uint64_t* stk) {
  return VFsIsDir((char const*)stk[0]);
}

static int64_t STK_DrawWindowUpdate(int64_t* stk) {
#ifndef HEADLESS
  DrawWindowUpdate((CDrawWindow*)stk[0], (int8_t*)stk[1], stk[2], stk[3]);
#endif
  return 0;
}

static int64_t STK___GetTicksHP() {
#ifndef _WIN32
  struct timespec ts;
  int64_t theTick = 0U;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  theTick = ts.tv_nsec / 1000;
  theTick += ts.tv_sec * 1000000U;
  return theTick;
#else
  static int64_t freq = 0;
  int64_t cur;
  if (!freq) {
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    freq /= 1000000U;
  }
  QueryPerformanceCounter((LARGE_INTEGER*)&cur);
  return cur / freq;
#endif
}

static uint64_t STK___GetTicks() {
  return GetTicks();
}

static int64_t STK_SetKBCallback(int64_t* stk) {
#ifndef HEADLESS
  SetKBCallback((void*)stk[0], (void*)stk[1]);
#endif
  return 0;
}

static int64_t STK_SetMSCallback(int64_t* stk) {
#ifndef HEADLESS
  SetMSCallback((void*)stk[0]);
#endif
  return 0;
}

static int64_t STK___AwakeCore(int64_t* stk) {
  AwakeFromSleeping(stk[0]);
  return 0;
}

static int64_t STK___SleepHP(int64_t* stk) {
  SleepHP(stk[0]);
  return 0;
}

static int64_t STK___Sleep(uint64_t* stk) {
  SleepHP(stk[0] * 1000);
  return 0;
}

static int64_t STK_SetFs(int64_t* stk) {
  SetFs((void*)stk[0]);
  return 0;
}

static int64_t STK_SetGs(int64_t* stk) {
  SetGs((void*)stk[0]);
  return 0;
}

static int64_t STK_SndFreq(uint64_t* stk) {
#ifndef HEADLESS
  SndFreq(stk[0]);
#endif
  return 0;
}

static int64_t STK_SetClipboardText(int64_t* stk) {
  // SDL_SetClipboardText(stk[0]);
#ifndef HEADLESS
  SetClipboard((char*)stk[0]);
#endif
  return 0;
}

static int64_t STK___GetStr(int64_t* stk) {
  char *s = NULL, *r = NULL;
#ifndef _WIN32
  s = linenoise((char*)stk[0]);
  if (!s)
    return (uintptr_t) nullptr;
  linenoiseHistoryAdd(s);
  r = HolyStrDup(s);
  free(s);
#else
  fputs("COMMAND LINE MODE IS NOT SUPPORTED FOR WINDOWS AT THE MOMENT", stderr);
  abort();
#endif
  return (int64_t)r;
}

static char* STK_GetClipboardText(int64_t* stk) {
#ifndef HEADLESS
  std::string clip{ClipboardText()};
  return HolyStrDup(clip.c_str());
#else
	return HolyStrDup("");
#endif
}

static int64_t STK_FUnixTime(int64_t* stk) {
  return VFsUnixTime((char*)stk[0]);
}

static uint64_t STK_VFsFTrunc(int64_t* stk) {
  fs::resize_file(VFsFileNameAbs((char*)stk[0]), stk[1]);
  return 0;
}

static int64_t STK___FExists(int64_t* stk) {
  return VFsFileExists((char*)stk[0]);
}

#ifndef _WIN32

#include <time.h>

static uint64_t STK_UnixNow(int64_t* stk) {
  return time(nullptr);
}

#else

static uint64_t STK_UnixNow(int64_t* stk) {
  int64_t r;
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  // https://archive.md/xl8qB
  uint64_t time = ft.dwLowDateTime | ((uint64_t)ft.dwHighDateTime << 32),
           adj = 10000 * (uint64_t)11644473600000;
  time -= adj;
  return time / 10000000ull;
}

#endif

uint64_t mp_cnt(int64_t*) {
  return thread::hardware_concurrency();
}

static void STK___SpawnCore(uint64_t* stk) {
  CreateCore(stk[0], (void*)stk[1]);
}

static void* STK_NewVirtualChunk(size_t* stk) {
  return NewVirtualChunk(stk[0], stk[1]);
}

static uint64_t STK_FreeVirtualChunk(int64_t* stk) {
  FreeVirtualChunk((void*)stk[0], stk[1]);
  return 0;
}

static uint64_t STK_VFsSetPwd(int64_t* stk) {
  VFsSetPwd((char*)stk[0]);
  return 1;
}

static uint64_t STK_VFsExists(int64_t* stk) {
  return VFsFileExists((char*)stk[0]);
}

static uint64_t STK_VFsIsDir(int64_t* stk) {
  return VFsIsDir((char*)stk[0]);
}

static int64_t STK_VFsFSize(int64_t* stk) {
  return VFsFSize((char*)stk[0]);
}

static uint64_t STK_VFsFRead(uintptr_t* stk) {
  return (uintptr_t)VFsFileRead((char const*)stk[0], (uint64_t* const)stk[1]);
}

static uint64_t STK_VFsFWrite(uintptr_t* stk) {
  return VFsFileWrite((char*)stk[0], (char*)stk[1], stk[2]);
}

static uint64_t STK_VFsDirMk(uintptr_t* stk) {
  return VFsDirMk((char*)stk[0], VFS_CDF_MAKE);
}

static uint64_t STK_VFsDir(uintptr_t* stk) {
  return (uintptr_t)VFsDir((char*)stk[0]);
}

static uint64_t STK_VFsDel(uintptr_t* stk) {
  return VFsDel((char*)stk[0]);
}

static uint64_t STK_VFsFOpenW(uintptr_t* stk) {
  return (uintptr_t)VFsFOpen((char*)stk[0], "w+b");
}

static uint64_t STK_VFsFOpenR(uintptr_t* stk) {
  return (uintptr_t)VFsFOpen((char*)stk[0], "rb");
}

static uint64_t STK_VFsFClose(uintptr_t* stk) {
  fclose((FILE*)stk[0]);
  return 0;
}

static int64_t STK_VFsFBlkRead(uintptr_t* stk) {
  fflush((FILE*)stk[3]);
  return stk[2] == fread((void*)stk[0], stk[1], stk[2], (FILE*)stk[3]);
}

static int64_t STK_VFsFBlkWrite(uintptr_t* stk) {
  bool r = stk[2] == fwrite((void*)stk[0], stk[1], stk[2], (FILE*)stk[3]);
  fflush((FILE*)stk[3]);
  return r;
}

static int64_t STK_VFsFSeek(int64_t* stk) {
  fseek((FILE*)stk[1], stk[0], SEEK_SET);
  return 0;
}

static int64_t STK_VFsSetDrv(int64_t* stk) {
  VFsSetDrv(stk[0]);
  return 0;
}

static int64_t STK_SetVolume(int64_t* stk) {
  static_assert(alignof(double) == alignof(uint64_t));
  union {
    double flt;
    int64_t i;
  } un;
  un.i = stk[0];
#ifndef HEADLESS
  SetVolume(un.flt);
#endif
  return 0;
}

static uint64_t STK_GetVolume(int64_t* stk) {
  union {
    double flt;
    int64_t i;
  } un;
#ifndef HEADLESS
  un.flt = GetVolume();
#endif
  return un.i;
}

static void STK_ExitTOS(int64_t* stk) {
  ShutdownTOS(stk[0]);
}

static void RegisterFunctionPtr(std::string& blob, char const* name, void* fp,
                                size_t arity) {
  // Function entry point offset from the code blob
  uintptr_t off = blob.size();
#ifdef _WIN32
  // clang-format off
  /*
  PUSH RBP
  MOV RBP,RSP
  AND RSP,-0x10
  PUSH R10
  PUSH R11
  SUB RSP,0x20 //Mandatory 4 stack arguments must be "pushed"
  LEA RCX,[RBP+8+8]
  PUSH R9
  PUSH R8
  PUSH RDX
  PUSH RCX
   */
  // clang-format on
  char const* atxt = "\x55\x48\x89\xE5"
                     "\x48\x83\xE4\xF0"
                     "\x41\x52\x41\x53"
                     "\x48\x83\xEC\x20"
                     "\x48\x8D\x4D\x10"
                     "\x41\x51\x41\x50"
                     "\x52\x51";
  blob.append(atxt, 26);
#else
  // clang-format off
  /*
  PUSH RBP
  MOV RBP,RSP //RBP will have point to the old RBP,as we just PUSHed it(We moved the pointer to the old RBP to the current RBP)
              //-0x10 =0xffffffffff0,16 is 0b1111 and AND ing will move the stack
              //down to an alignment of 16(it chops off the bits)
  AND RSP,-0x10 //This will align the stack to 16
                //SysV OS will save R12-15 which are needed,but TempleOS needs to save
                //RSI,RDI,R10-15
  PUSH RSI
  PUSH RDI
  PUSH R10
  PUSH R11
//Load Effective Address
// RBP+16 This is where TempleOS puts the argument
// RBP+8 the return address(When you CALL a function,it pushes the return address(RIP) to the stack)
// RBP+0 The old RBP
  LEA RDI,[RBP+8+8] //RDI=&RBP+8+8 Because at RBP+16 we have the first stack argument
 */
  // clang-format on
  char const* atxt = "\x55\x48\x89\xE5"
                     "\x48\x83\xE4\xF0"
                     "\x56\x57\x41\x52"
                     "\x41\x53\x48\x8D"
                     "\x7D\x10";
  blob.append(atxt, 18);
#endif
  // MOV RAX,fptr
  atxt = "\x48\xb8";
  blob.append(atxt, 2);
  // clang-format off
  for (uint8_t i = 0; i < 8; ++i)
    blob.push_back(0xff & (reinterpret_cast<uintptr_t>(fp) >> i*8));
#ifdef _WIN32
  /*
  CALL RAX
  ADD RSP,0x40
  POP R11
  POP R10
  LEAVE
  */
  atxt = "\xFF\xD0\x48\x83"
	 "\xC4\x40\x41\x5B"
	 "\x41\x5A\xC9";
  blob.append(atxt, 11);	
#else
  /*
  CALL RAX
  POP R11
  POP R10
  POP RDI
  POP RSI
  LEAVE //This instruction will move RSP to the old base ptr and POP RBP
        //It is the same as this
        //   MOV RSP,RBP //Our old RBP address on the stack
        //   POP RBP
  */
  atxt = "\xFF\xD0\x41\x5B"
	 "\x41\x5A\x5F\x5E"
	 "\xC9";
  blob.append(atxt, 9);
#endif
  // RET1 will pop the old return address from the stack,AND it will remove the
  // arguments' off the stack
  // RET1 is like this
  // POP RIP
  // ADD RSP,cnt //Remove cnt bytes from the stack
  //
  // RET1 ARITY*8 (8 == sizeof(uint64_t))
  // HolyC ABI is __stdcall, the callee cleans up its own stack
  // unless its variadic
  //
  // A bit about HolyC ABI: all args are 8 bytes(64 bits)
  // let there be function Foo(I64 i, ...);
  // Foo(2, 4, 5, 6)
  //   argv[2] 6 // RBP + 48
  //   argv[1] 5 // RBP + 40
  //   argv[0] 4 // RBP + 32 <-points- argv(internal var in function)
  //   argc 3(num of varargs) // RBP + 24 <-value- argc(internal var in function)
  //   i  2    // RBP + 16(this is where the stack starts)
  // clang-format on
  blob.push_back('\xc2');
  arity *= 8;
  // Arity is 16bits in the instrction(64 kilobytes of arguments and below ONLY)
  blob.push_back(arity & 0xFF);
  blob.push_back((arity >> 8) & 0xFF);
  CHash sym;
  sym.type = HTT_FUN;
  sym.val = reinterpret_cast<void*>(off);
  TOSLoader[name].emplace_back(sym);
}

#ifdef HEADLESS
struct CDrawWindow;
CDrawWindow* NewDrawWindow() {
	return NULL;
}
#endif

void RegisterFuncPtrs() {
  std::string ffi_blob;
#define R_(holy, secular, arity) \
  RegisterFunctionPtr(ffi_blob, holy, reinterpret_cast<void*>(secular), arity)
#define S_(name, arity)                                                     \
  RegisterFunctionPtr(ffi_blob, #name, reinterpret_cast<void*>(STK_##name), \
                      arity)
  R_("__CmdLineBootText", CmdLineBootText, 0);
  R_("__IsCmdLine", IsCmdLine, 0);
  R_("mp_cnt", mp_cnt, 0);
  R_("__CoreNum", CoreNum, 0);
  R_("GetFs", GetFs, 0);
  R_("GetGs", GetGs, 0);
  R_("DrawWindowNew", NewDrawWindow, 0);
  S_(__IsValidPtr, 1);
  S_(__SpawnCore, 0);
  S_(UnixNow, 0);
  S_(InterruptCore, 1);
  S_(NewVirtualChunk, 2);
  S_(FreeVirtualChunk, 2);
  S_(ExitTOS, 1);
  S_(__GetStr, 1);
  S_(__FExists, 1);
  S_(FUnixTime, 1);
  S_(SetClipboardText, 1);
  S_(GetClipboardText, 0);
  S_(SndFreq, 1);
  S_(__Sleep, 1);
  S_(__SleepHP, 1);
  S_(__AwakeCore, 1);
  S_(SetFs, 1);
  S_(SetGs, 1);
  S_(SetKBCallback, 2);
  S_(SetMSCallback, 1);
  S_(__GetTicks, 0);
  S_(__BootstrapForeachSymbol, 1);
  S_(IsDir, 1);
  S_(DrawWindowUpdate, 4);
  S_(UnblockSignals, 0);
  /*
   * In TempleOS variadics, functions follow __cdecl, whereas normally
   * they follow __stdcall which is why the arity argument is needed(RET1 x).
   * Thus we don't have to clean up the stack in variadics.
   */
  S_(TOSPrint, 0);
  S_(DyadInit, 0);
  S_(DyadUpdate, 0);
  S_(DyadShutdown, 0);
  S_(DyadNewStream, 0);
  S_(DyadListen, 2);
  S_(DyadConnect, 3);
  S_(DyadWrite, 3);
  S_(DyadEnd, 1);
  S_(DyadClose, 1);
  S_(DyadGetAddress, 1);
  S_(DyadSetReadCallback, 3);
  S_(DyadSetOnListenCallback, 3);
  S_(DyadSetOnConnectCallback, 3);
  S_(DyadSetOnCloseCallback, 3);
  S_(DyadSetOnReadyCallback, 3);
  S_(DyadSetOnTimeoutCallback, 3);
  S_(DyadSetOnTickCallback, 3);
  S_(DyadSetOnErrorCallback, 3);
  S_(DyadSetOnDestroyCallback, 3);
  S_(DyadSetTimeout, 2);
  S_(DyadSetNoDelay, 2);
  S_(VFsFTrunc, 2);
  S_(VFsSetPwd, 1);
  S_(VFsExists, 1);
  S_(VFsIsDir, 1);
  S_(VFsFSize, 1);
  S_(VFsFRead, 2);
  S_(VFsFWrite, 3);
  S_(VFsDel, 1);
  S_(VFsDir, 0);
  S_(VFsDirMk, 1);
  S_(VFsFBlkRead, 4);
  S_(VFsFBlkWrite, 4);
  S_(VFsFOpenW, 1);
  S_(VFsFOpenR, 1);
  S_(VFsFClose, 1);
  S_(VFsFSeek, 2);
  S_(VFsSetDrv, 1);
  S_(GetVolume, 0);
  S_(SetVolume, 1);
  S_(__GetTicksHP, 0);
  S_(_GrPaletteColorSet, 2);
  S_(UVBufBase, 1);
  S_(UVBufLen, 1);
  S_(UVRandomNew, 0);
  S_(UVRandomDel, 1);
  S_(UVLoopNew, 0);
  S_(UVLoopDel, 1);
  S_(UVRun, 2);
  auto blob = static_cast<char*>(NewVirtualChunk(ffi_blob.size(), true));
  std::copy(ffi_blob.begin(), ffi_blob.end(), blob);
  for (auto& m : TOSLoader) {
    auto& [symname, vec] = m;
    if (vec.size() == 0)
      continue;
    auto& sym = vec[0];
    sym.val = blob + (uintptr_t)sym.val;
  }
}
