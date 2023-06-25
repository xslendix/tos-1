#include "TOSPrint.hxx"
#include "ffi.h"
#include "main.hxx"
#include "mem.hxx"
#include "multic.hxx"
#include "sdl_window.hxx"
#include "sound.h"
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
  static int64_t i;
  if (!i) {
    i = 1;
    dyad_init();
    dyad_setUpdateTimeout(0.);
  }
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
static void STK_DyadSetCloseCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_CLOSE, &DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetConnectCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_CONNECT, &DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetDestroyCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_DESTROY, &DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetErrorCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_ERROR, &DyadCloseCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetReadyCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_READY, &DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetTickCallback(int64_t* stk) {
  dyad_addListener((dyad_Stream*)stk[0], DYAD_EVENT_TICK, &DyadListenCB,
                   (void*)stk[1], (void*)stk[2]);
}
static void STK_DyadSetTimeoutCallback(int64_t* stk) {
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

static void STK__3DaysGrPaletteColorSet(int64_t* stk) {
  GrPaletteColorSet(stk[0], stk[1]);
}

static int64_t IsValidPtr(int64_t* stk) {
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
  static size_t ps;
  if (!ps)
    ps = getpagesize();
  stk[0] /= ps; // align to
  stk[0] *= ps; // page boundary
  // https://archive.md/Aj0S4
  return -1 != msync((void*)stk[0], ps, MS_ASYNC);
  /*#elif defined(__linux__)
        // TOO FUCKING GODDAMN SLOW!!!!!
    auto constexpr Hex2I64 = [](char const *ptr, char const** res) {
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
    };
    std::ifstream map{"/proc/self/maps", ios::binary | ios::in};
    std::string s, buffer;
    while (std::getline(map, buffer))
      (s += buffer) += '\n';
    char const *ptr = s.c_str();
    while (*ptr) {
      auto lower = Hex2I64(ptr, &ptr);
      ++ptr; // skip '-'
      auto upper = Hex2I64(ptr, &ptr);
      if (lower <= stk[0] && stk[0] <= upper)
        return 1;
      ptr = strchr(ptr, '\n');
      if (ptr == nullptr)
        return 0;
      ++ptr; // go to next line
    }
    return 0;
  #endif*/

#endif
}

static void STK_InterruptCore(int64_t* stk) {
  InterruptCore(stk[0]);
}

static void STK_ForeachFunc(int64_t* stk) {
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
  DrawWindowUpdate((CDrawWindow*)stk[0], (int8_t*)stk[1], stk[2], stk[3]);
  return 0;
}

int64_t STK__GetTicksHP() {
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

int64_t STK___GetTicks() {
  return GetTicks();
}

int64_t STK_SetKBCallback(int64_t* stk) {
  SetKBCallback((void*)stk[0], (void*)stk[1]);
  return 0;
}

int64_t STK_SetMSCallback(int64_t* stk) {
  SetMSCallback((void*)stk[0]);
  return 0;
}

int64_t STK_AwakeFromSleeping(int64_t* stk) {
  AwakeFromSleeping(stk[0]);
  return 0;
}

int64_t STK_SleepHP(int64_t* stk) {
  SleepHP(stk[0]);
  return 0;
}

int64_t STK_Sleep(int64_t* stk) {
  SleepHP((uint64_t)stk[0] * 1000);
  return 0;
}

int64_t STK_SetFs(int64_t* stk) {
  SetFs((void*)stk[0]);
  return 0;
}

int64_t STK_SetGs(int64_t* stk) {
  SetGs((void*)stk[0]);
  return 0;
}

int64_t STK_SndFreq(uint64_t* stk) {
  SndFreq(stk[0]);
  return 0;
}
int64_t STK_SetClipboardText(int64_t* stk) {
  // SDL_SetClipboardText(stk[0]);
  SetClipboard((char*)stk[0]);
  return 0;
}

int64_t STK___GetStr(int64_t* stk) {
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

int64_t STK_GetClipboardText(int64_t* stk) {
  char* r = ClipboardText();
  char* r2 = HolyStrDup(r);
  free(r);
  return (int64_t)r2;
}

int64_t STK_FUnixTime(int64_t* stk) {
  return VFsUnixTime((char*)stk[0]);
}

int64_t STK_FTrunc(int64_t* stk) {
  fs::resize_file(VFsFileNameAbs((char*)stk[0]), stk[1]);
  return 0;
}

int64_t STK___FExists(int64_t* stk) {
  return VFsFileExists((char*)stk[0]);
}

#ifndef _WIN32

#include <time.h>

uint64_t STK_Now(int64_t* stk) {
  return time(nullptr);
}

#else

uint64_t STK_Now(int64_t* stk) {
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

void SpawnCore(int64_t* stk) {
  CreateCore(stk[0], (void*)stk[1]);
}

int64_t STK_NewVirtualChunk(int64_t* stk) {
  return (int64_t)NewVirtualChunk(stk[0], stk[1]);
}

int64_t STK_FreeVirtualChunk(int64_t* stk) {
  FreeVirtualChunk((void*)stk[0], stk[1]);
  return 0;
}

int64_t STK_VFsSetPwd(int64_t* stk) {
  VFsSetPwd((char*)stk[0]);
  return 1;
}

int64_t STK_VFsExists(int64_t* stk) {
  return VFsFileExists((char*)stk[0]);
}

int64_t STK_VFsIsDir(int64_t* stk) {
  return VFsIsDir((char*)stk[0]);
}

int64_t STK_VFsFileSize(int64_t* stk) {
  return VFsFSize((char*)stk[0]);
}

int64_t STK_VFsFRead(int64_t* stk) {
  return (intptr_t)VFsFileRead((char const*)stk[0], (uint64_t* const)stk[1]);
}

int64_t STK_VFsFWrite(int64_t* stk) {
  return VFsFileWrite((char*)stk[0], (char*)stk[1], stk[2]);
}

int64_t STK_VFsDirMk(int64_t* stk) {
  return VFsCd((char*)stk[0], VFS_CDF_MAKE);
}

int64_t STK_VFsDir(int64_t* stk) {
  return (int64_t)VFsDir((char*)stk[0]);
}

int64_t STK_VFsDel(int64_t* stk) {
  return VFsDel((char*)stk[0]);
}

int64_t STK_VFsFOpenW(int64_t* stk) {
  return (intptr_t)VFsFOpen((char*)stk[0], "w+b");
}

int64_t STK_VFsFOpenR(int64_t* stk) {
  return (intptr_t)VFsFOpen((char*)stk[0], "rb");
}

int64_t STK_VFsFClose(int64_t* stk) {
  fclose((FILE*)stk[0]);
  return 0;
}

int64_t STK_VFsFBlkRead(int64_t* stk) {
  fflush((FILE*)stk[3]);
  return stk[2] == fread((void*)stk[0], stk[1], stk[2], (FILE*)stk[3]);
}

int64_t STK_VFsFBlkWrite(int64_t* stk) {
  bool r = stk[2] == fwrite((void*)stk[0], stk[1], stk[2], (FILE*)stk[3]);
  fflush((FILE*)stk[3]);
  return r;
}

int64_t STK_VFsFSeek(int64_t* stk) {
  fseek((FILE*)stk[1], stk[0], SEEK_SET);
  return 0;
}

int64_t STK_VFsDrv(int64_t* stk) {
  VFsSetDrv(stk[0]);
  return 0;
}

int64_t STK_SetVolume(int64_t* stk) {
  static_assert(alignof(double) == alignof(uint64_t));
  union {
    double flt;
    int64_t i;
  } un;
  un.i = stk[0];
  SetVolume(un.flt);
  return 0;
}

uint64_t STK_GetVolume(int64_t* stk) {
  union {
    double flt;
    int64_t i;
  } un;
  un.flt = GetVolume();
  return un.i;
}

static void STK_ExitTOS(int64_t* stk) {
  ShutdownTOS(stk[0]);
}

static void RegisterFunctionPtr(std::string& blob, char const* name, void* fp,
                                size_t arity) {
  auto sz = blob.size();
#ifdef _WIN32
  /*
  PUSH RBP
  MOV RBP,RSP
  AND RSP,-0x10
  PUSH R10
  PUSH R11
  SUB RSP,0x20 //Manditory 4 stack
  arguments must be "pushed" LEA
  RCX,[RBP+8+8] PUSH R9 PUSH R8 PUSH RDX
  PUSH RCX
   */
  char const* atxt = "\x55\x48\x89\xE5"
                     "\x48\x83\xE4\xF0"
                     "\x41\x52\x41\x53"
                     "\x48\x83\xEC\x20"
                     "\x48\x8D\x4D\x10"
                     "\x41\x51\x41\x50"
                     "\x52\x51";
  blob.append(atxt, 26);
#else
  /*
PUSH RBP
MOV RBP,RSP //RBP will have point to the old RBP,as we just PUSHed it(We moved
the pointer to the old RBP to the current RBP)
//-0x10 =0xffffffffff0,16 is 0b1111 and AND ing will move the stack down to an
alignment of 16(it chops off the bits) AND RSP,-0x10 //This will align the
stack to 16
//SysV OS will save R12-15 which are needed,but TempleOS needs to save
RSI,RDI,R10-15
// PUSH RSI PUSH RDI PUSH R10 PUSH R11
//Load Effective Address
// RBP+16 This is where TempleOS puts the argument
// RBP+8 the return address(When you CALL a function,it pushes the return
address(RIP) to the stack)
// RBP+0 The old RBP
LEA RDI,[RBP+8+8] RDI=&RBP+8+8 Because at RBP+16 we have the first stack
argument
 */
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
  //This instruction will move RSP to the old base ptr and POP RBP
  //It is the same as this
  //   MOV RSP,RBP //Our old RBP address on the stack
  //   POP RBP
  LEAVE
  */
  atxt = "\xFF\xD0\x41\x5B"
	 "\x41\x5A\x5F\x5E"
	 "\xC9";
  blob.append(atxt, 9);
  // clang-format on
#endif
  // RET1 will pop the old return address from the stack,AND it will remove the
  // arguments' off the stack
  // RET1 is like this
  // POP RIP
  // ADD RSP,cnt //Remove cnt bytes from the stack
  //
  // RET1 ARITY*8
  blob.push_back('\xc2');
  arity *= 8;
  // Arity is 16bits in the instrction(64 kilobytes of arguments and below ONLY)
  blob.push_back(arity & 0xFF);
  blob.push_back((arity >> 8) & 0xFF);
  CHash sym;
  sym.type = HTT_FUN;
  sym.val = (void*)(uintptr_t)sz;
  TOSLoader[name].emplace_back(sym);
}

void RegisterFuncPtrs() {
  std::string ffi_blob;
#define R_(holy, secular, arity) \
  RegisterFunctionPtr(ffi_blob, holy, reinterpret_cast<void*>(secular), arity)
  R_("UnixNow", STK_Now, 0);
  R_("InterruptCore", STK_InterruptCore, 1);
  R_("NewVirtualChunk", STK_NewVirtualChunk, 2);
  R_("FreeVirtualChunk", STK_FreeVirtualChunk, 2);
  R_("__CmdLineBootText", CmdLineBootText, 0);
  R_("Exit3Days", STK_ExitTOS, 1);
  R_("ExitTOS", STK_ExitTOS, 1);
  R_("__GetStr", STK___GetStr, 1);
  R_("__IsCmdLine", IsCmdLine, 0);
  R_("__FExists", STK___FExists, 1);
  R_("mp_cnt", mp_cnt, 0);
  R_("__SpawnCore", SpawnCore, 2);
  R_("__CoreNum", CoreNum, 0);
  R_("FUnixTime", STK_FUnixTime, 1);
  R_("SetClipboardText", STK_SetClipboardText, 1);
  R_("GetClipboardText", STK_GetClipboardText, 0);
  R_("SndFreq", STK_SndFreq, 1);
  R_("__Sleep", &STK_Sleep, 1);
  R_("__SleepHP", &STK_SleepHP, 1);
  R_("__AwakeCore", &STK_AwakeFromSleeping, 1);
  R_("GetFs", GetFs, 0);
  R_("SetFs", STK_SetFs, 1);
  R_("GetGs", GetGs, 0);
  R_("SetGs", STK_SetGs, 1);
  R_("SetKBCallback", STK_SetKBCallback, 2);
  R_("SetMSCallback", STK_SetMSCallback, 1);
  R_("__GetTicks", STK___GetTicks, 0);
  R_("__IsValidPtr", IsValidPtr, 1);
  R_("__BootstrapForeachSymbol", STK_ForeachFunc, 1);
  R_("IsDir", STK_IsDir, 1);
  R_("DrawWindowUpdate", STK_DrawWindowUpdate, 4);
  R_("DrawWindowNew", NewDrawWindow, 0);
  R_("UnblockSignals", STK_UnblockSignals, 0);
  /*
   * In TempleOS variadics, functions follow __cdecl, whereas normally
   * they follow __stdcall which is why the arity argument is needed(RET1 x).
   * Thus we don't have to clean up the stack in variadics.
   */
  R_("TOSPrint", STK_TOSPrint, 0);

  R_("DyadInit", &STK_DyadInit, 0);
  R_("DyadUpdate", &STK_DyadUpdate, 0);
  R_("DyadShutdown", &STK_DyadShutdown, 0);
  R_("DyadNewStream", &STK_DyadNewStream, 0);
  R_("DyadListen", &STK_DyadListen, 2);
  R_("DyadConnect", &STK_DyadConnect, 3);
  R_("DyadWrite", &STK_DyadWrite, 3);
  R_("DyadEnd", &STK_DyadEnd, 1);
  R_("DyadClose", &STK_DyadClose, 1);
  R_("DyadGetAddress", STK_DyadGetAddress, 1);
  R_("DyadSetReadCallback", STK_DyadSetReadCallback, 3);
  R_("DyadSetOnListenCallback", STK_DyadSetOnListenCallback, 3);
  R_("DyadSetOnConnectCallback", STK_DyadSetConnectCallback, 3);
  R_("DyadSetOnCloseCallback", STK_DyadSetCloseCallback, 3);
  R_("DyadSetOnReadyCallback", STK_DyadSetReadyCallback, 3);
  R_("DyadSetOnTimeoutCallback", STK_DyadSetTimeoutCallback, 3);
  R_("DyadSetOnTickCallback", STK_DyadSetTimeoutCallback, 3);
  R_("DyadSetOnErrorCallback", STK_DyadSetErrorCallback, 3);
  R_("DyadSetOnDestroyCallback", STK_DyadSetDestroyCallback, 3);
  R_("DyadSetTimeout", STK_DyadSetTimeout, 2);
  R_("DyadSetNoDelay", STK_DyadSetNoDelay, 2);
  R_("VFsFTrunc", STK_FTrunc, 2);
  R_("VFsSetPwd", STK_VFsSetPwd, 1);
  R_("VFsExists", STK_VFsExists, 1);
  R_("VFsIsDir", STK_VFsIsDir, 1);
  R_("VFsFSize", STK_VFsFileSize, 1);
  R_("VFsFRead", STK_VFsFRead, 2);
  R_("VFsFWrite", STK_VFsFWrite, 3);
  R_("VFsDel", STK_VFsDel, 1);
  R_("VFsDir", STK_VFsDir, 0);
  R_("VFsDirMk", STK_VFsDirMk, 1);
  R_("VFsFBlkRead", STK_VFsFBlkRead, 4);
  R_("VFsFBlkWrite", STK_VFsFBlkWrite, 4);
  R_("VFsFOpenW", STK_VFsFOpenW, 1);
  R_("VFsFOpenR", STK_VFsFOpenR, 1);
  R_("VFsFClose", STK_VFsFClose, 1);
  R_("VFsFSeek", STK_VFsFSeek, 2);
  R_("VFsSetDrv", STK_VFsDrv, 1);
  R_("GetVolume", STK_GetVolume, 0);
  R_("SetVolume", STK_SetVolume, 1);
  R_("__GetTicksHP", STK__GetTicksHP, 0);
  R_("_3DaysGrPaletteColorSet", STK__3DaysGrPaletteColorSet, 2);
  R_("UVBufBase", STK_UVBufBase, 1);
  R_("UVBufLen", STK_UVBufLen, 1);
  R_("UVRandom", STK_UVRandom, 6);
  R_("UVRandomNew", STK_UVRandomNew, 0);
  R_("UVRandomDel", STK_UVRandomDel, 1);
  R_("UVLoopNew", STK_UVLoopNew, 0);
  R_("UVLoopDel", STK_UVLoopDel, 1);
  R_("UVRun", STK_UVRun, 2);
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
