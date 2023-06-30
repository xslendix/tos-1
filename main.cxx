#include "main.hxx"
#include "ffi.h"
#include "multic.hxx"
#include "runtime.hxx"
#ifndef HEADLESS
#include "sdl_window.hxx"
#include "sound.h"
#endif
#include "tos_aot.hxx"
#include "vfs.hxx"
// its dangerous and lonely out here socrates

#include "ext/argtable3.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <utility>
namespace fs = std::filesystem;

static constexpr bool is_win =
#ifdef _WIN32
    true;
#else
    false;
#endif

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <winbase.h>
#include <wincon.h>
#include <processenv.h>
#include <processthreadsapi.h>
// clang-format on

static BOOL WINAPI CtrlCHandlerRoutine(DWORD) {
#define STR_(x) x, lstrlenA(x)
  WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), STR_("User Abort.\n"), nullptr,
                nullptr);
  return TRUE;
}

#else
#include <signal.h>
#endif

static struct arg_lit *helpArg, *sixty_fps, *commandLineArg, *cb_sanitize;
static struct arg_lit *silent, *noans;
static struct arg_file *cmdLineFiles, *TDriveArg, *HCRTArg;

static std::string bin_path{"HCRT.BIN"};
static void* Core0(void*) {
  VFsThrdInit();
#ifndef _WIN32
  signal(SIGUSR1, [](int) {
    FFI_CALL_TOS_0(TOSLoader["__FreeCPUs"][0].val);
    pthread_exit(nullptr);
  });
#endif
  LoadHCRT(bin_path);
  return nullptr;
}

static bool is_cmd_line = false;
bool IsCmdLine() {
  return is_cmd_line;
}

static std::string boot_str;
char const* CmdLineBootText() {
  return boot_str.c_str();
}

static bool prog_exit = false;
void ShutdownTOS(int32_t ec) {
  prog_exit = true;
  ShutdownCores(ec);
}

bool sanitize_clipboard = false;
int main(int argc, char** argv) {
  void* argtable[] = {
      helpArg = arg_lit0("h", "help", "Display this help message."),
#ifndef HEADLESS
      sixty_fps = arg_lit0("6", "60fps", "Run in 60 fps mode."),
      commandLineArg = arg_lit0("c", "com",
                                "Start in command line "
                                "mode,mount drive '/' at /."),
#endif
      HCRTArg = arg_file0("f", "file", nullptr,
                          "Specifies where your HolyC runtime is"),
      TDriveArg = arg_file0("t", nullptr, "T(boot) Drive",
                            "This tells tos where to "
                            "use(or create) the boot "
                            "drive folder."),
      cmdLineFiles = arg_filen(nullptr, nullptr, "<files>", 0, 100,
                               "Files for use with command "
                               "line mode."),
      cb_sanitize = arg_lit0("s", "sanitize-cb",
                             "Sanitizes clipboard "
                             "contents(disabled by "
                             "default to allow "
                             "pasting DolDoc contents,use "
                             "this to allow possible page "
                             "faults when pasting UTF-8 "
                             "sequences due to collision "
                             "with "
                             "DolDoc control chars)"),
      silent = arg_lit0("s", "silent", "Silence compiler output"),
      noans = arg_lit0("a", "noans", "No answer"),
      arg_end_(1),
  };
  int errs = arg_parse(argc, argv, argtable);
  if (helpArg->count > 0 || errs != 0 || TDriveArg->count == 0) {
    std::cerr << "Usage is: " << argv[0];
    arg_print_syntaxv(stderr, argtable, "\n");
    arg_print_glossary(stderr, argtable, "  %-25s %s\n");
    return 1;
  }
  if (fs::exists(TDriveArg->filename[0])) {
    VFsMountDrive('T', TDriveArg->filename[0]);
  } else {
    std::cerr << TDriveArg->filename[0] << " DOES NOT EXIST\n";
    exit(1);
  }
  // make drive T
#ifndef HEADLESS
  if (commandLineArg->count)
#endif
    VFsMountDrive('Z', ".");
  VFsThrdInit();
  // This is called before LoadHCRT so TOSLoader will not be
  // all fucked up, fyi
  RegisterFuncPtrs();
#ifndef HEADLESS
  if (commandLineArg->count > 0) {
#endif
    is_cmd_line = true;
    boot_str += "Cd(\"Z:/\");\n";
    for (int i = 0; i < cmdLineFiles->count; ++i) {
      boot_str += "#include \"";
      boot_str += cmdLineFiles->filename[i];
      boot_str += "\";\n";
    }
#ifdef _WIN32
    std::replace(boot_str.begin(), boot_str.end(), '\\', '/');
#endif
  if (!silent->count)
    boot_str += "EnableDebug;;\n";
  if (!noans->count)
    boot_str += "EnableAnswer;;\n";
#ifndef HEADLESS
  }
  if (sixty_fps->count)
    boot_str += "SetFPS(60.);;\n";
  if (!is_cmd_line)
    NewDrawWindow();
  if (is_win || !is_cmd_line)
    InitSound();
#endif
  if (HCRTArg->count > 0)
    bin_path = HCRTArg->filename[0];
  if (fs::exists(bin_path)) {
		if (silent->count != 0)
			std::cerr << "Using " << bin_path << " as the default binary.\n";
    LaunchCore0(Core0);
  } else {
    std::cerr << bin_path << " DOES NOT EXIST\n";
    return 1;
  }
#ifndef HEADLESS
  if (!is_cmd_line) {
#ifdef _WIN32
    SetConsoleCtrlHandler(CtrlCHandlerRoutine, TRUE);
#endif
    if (cb_sanitize->count > 0)
      sanitize_clipboard = true;
    InputLoop(&prog_exit);
  } else {
#endif
    WaitForCore0();
    FFI_CALL_TOS_0(TOSLoader["__FreeCPUs"][0].val);
#ifndef HEADLESS
  }
#endif
  return 0;
}
