#include "dbg.hxx"
#include "ffi.h"
#include "tos_aot.hxx"

#ifdef _WIN32

#include <errhandlingapi.h>
#include <windows.h>

static LONG WINAPI VectorHandler(struct _EXCEPTION_POINTERS* info) {
  auto c = info->ExceptionRecord->ExceptionCode;
  switch (c) {
#define FERR(code)       \
  case EXCEPTION_##code: \
    break;
    FERR(ACCESS_VIOLATION);
    FERR(ARRAY_BOUNDS_EXCEEDED);
    FERR(DATATYPE_MISALIGNMENT);
    FERR(FLT_DENORMAL_OPERAND);
    FERR(FLT_DIVIDE_BY_ZERO);
    FERR(FLT_INEXACT_RESULT);
    FERR(FLT_INVALID_OPERATION);
    FERR(FLT_OVERFLOW);
    FERR(FLT_STACK_CHECK);
    FERR(FLT_UNDERFLOW);
    FERR(ILLEGAL_INSTRUCTION);
    FERR(IN_PAGE_ERROR);
    FERR(INT_DIVIDE_BY_ZERO);
    FERR(INVALID_DISPOSITION);
    FERR(STACK_OVERFLOW);
    FERR(BREAKPOINT);
  // https://archive.md/sZzVj
  case STATUS_SINGLE_STEP:
    break;
  default:
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  CONTEXT* ctx = info->ContextRecord;
#define REG(x) ctx->x
  uint64_t regs[] = {
      REG(Rax),    REG(Rcx), REG(Rdx),
      REG(Rbx),    REG(Rsp), REG(Rbp),
      REG(Rsi),    REG(Rdi), REG(R8),
      REG(R9),     REG(R10), REG(R11),
      REG(R12),    REG(R13), REG(R14),
      REG(R15),    REG(Rip), (uintptr_t)&ctx->FltSave,
      REG(EFlags),
  };
  uint64_t sig = (c == EXCEPTION_BREAKPOINT || c == STATUS_SINGLE_STEP)
                   ? 5 /* SIGTRAP */
                   : 0;
  FFI_CALL_TOS_2(TOSLoader["DebuggerLandWin"][0].val, sig, (uintptr_t)regs);
  return EXCEPTION_CONTINUE_EXECUTION;
}

void SetupDebugger() {
  AddVectoredExceptionHandler(1, &VectorHandler);
}

#else

#include <signal.h>
#include <ucontext.h>

// apparently mcontext is implementation defined idk
// if your on musl or something fix this yourself and send me a patch
/*enum {
  REG_R8 = 0,
  REG_R9,
  REG_R10,
  REG_R11,
  REG_R12,
  REG_R13,
  REG_R14,
  REG_R15,
  REG_RDI,
  REG_RSI,
  REG_RBP,
  REG_RBX,
  REG_RDX,
  REG_RAX,
  REG_RCX,
  REG_RSP,
  REG_RIP,
  REG_EFL,
  REG_CSGSFS, // segment regs CS, GS, FS
              // (each 16bit, padding at the end)
  REG_ERR,
  REG_TRAPNO,
  REG_OLDMASK,
  REG_CR2,
};*/

static void routine(int sig, siginfo_t* info, ucontext_t* ctx) {
  BackTrace();
  uint64_t sig_i64 = sig;
#ifdef __linux__
#define REG(x) static_cast<uint64_t>(ctx->uc_mcontext.gregs[REG_##x])
  // probably only works on glibc lmao
  // clang-format off
  // heres why i dont take the address of fpregs on linux
  // https://github.com/bminor/glibc/blob/4290aed05135ae4c0272006442d147f2155e70d7/sysdeps/unix/sysv/linux/x86/sys/ucontext.h#L239
  // clang-format on
  uint64_t regs[] = {
      REG(RAX), REG(RCX), REG(RDX),
      REG(RBX), REG(RSP), REG(RBP),
      REG(RSI), REG(RDI), REG(R8),
      REG(R9),  REG(R10), REG(R11),
      REG(R12), REG(R13), REG(R14),
      REG(R15), REG(RIP), (uintptr_t)ctx->uc_mcontext.fpregs,
      REG(EFL),
  };
#elif defined(__FreeBSD__)
#define REG(X) static_cast<uint64_t>(ctx->uc_mcontext.mc_##X)
  // freebsd seems to just use an
  // array of longs for their floating point context lmao
  uint64_t regs[] = {
      REG(rax),    REG(rcx), REG(rdx),
      REG(rbx),    REG(rsp), REG(rbp),
      REG(rsi),    REG(rdi), REG(r8),
      REG(r9),     REG(r10), REG(r11),
      REG(r12),    REG(r13), REG(r14),
      REG(r15),    REG(rip), (uintptr_t)&ctx->uc_mcontext.mc_fpstate,
      REG(rflags),
  };
#endif
  FFI_CALL_TOS_2(TOSLoader["DebuggerLand"][0].val, sig_i64, (uintptr_t)regs);
}

void SetupDebugger() {
  struct sigaction inf;
  inf.sa_flags = SA_SIGINFO | SA_NODEFER;
  inf.sa_sigaction = (void (*)(int, siginfo_t*, void*))routine;
  sigemptyset(&inf.sa_mask);
  sigaction(SIGTRAP, &inf, nullptr);
  sigaction(SIGBUS, &inf, nullptr);
  sigaction(SIGSEGV, &inf, nullptr);
  sigaction(SIGFPE, &inf, nullptr);
}

#endif
