#include "vfs.hxx"
#include "runtime.hxx"

#include <filesystem>
#include <string>
#include <vector>
namespace fs = std::filesystem;
#include <algorithm>
#include <fstream>

using std::ios;

#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <processthreadsapi.h>
#include <synchapi.h>
#include <windows.h>

#endif

#ifdef _WIN32
#include <fileapi.h>
#include <windows.h>
#define delim '\\'
#else
#define delim '/'
#include <sys/stat.h>
#include <sys/types.h>
#endif

thread_local std::string thrd_pwd;
thread_local char thrd_drv;

void VFsThrdInit() {
  thrd_pwd = "/";
  thrd_drv = 'T';
}

void VFsSetDrv(char const d) {
  if (!isalpha(d))
    return;
  thrd_drv = toupper(d);
}

void VFsSetPwd(char const* pwd) {
  if (!pwd)
    pwd = "/";
  thrd_pwd = pwd;
}

static bool FExists(std::string const& path) {
  return fs::exists(path);
}

static int FIsDir(std::string const& path) {
  return fs::is_directory(path);
}

uint64_t VFsDirMk(char const* to, int const flags) {
  std::string p = VFsFileNameAbs(to);
  if (FExists(p) && FIsDir(p)) {
    return 1;
  } else if (flags & VFS_CDF_MAKE) {
    fs::create_directory(p);
    return 1;
  }
  return 0;
}

uint64_t VFsDel(char const* p) {
  std::string path = VFsFileNameAbs(p);
  bool e = FExists(path);
  if (!e)
    return 0;
  fs::remove_all(path);
  return 1;
}

static std::string mount_points['z' - 'a' + 1];
std::string VFsFileNameAbs(char const* name) {
  std::string ret;
  // thrd_drv is always uppercase
  ret += mount_points[thrd_drv - 'A']; // T
  ret += delim;                        // /
  if (thrd_pwd.size() > 1) {
    ret.pop_back();
    ret += thrd_pwd; // /
    ret += delim;    // /
  }
  ret += name; // Name
  return ret;
}

int64_t VFsFSize(char const* name) {
  std::string fn = VFsFileNameAbs(name);
  if (!FExists(fn)) {
    return -1;
  } else if (FIsDir(fn)) {
    fs::directory_iterator it{fn};
    return std::distance(fs::begin(it), fs::end(it));
  }
  return fs::file_size(fn);
}

#ifndef _WIN32

int64_t VFsUnixTime(char const* name) {
  std::string fn = VFsFileNameAbs(name);
  struct stat s;
  stat(fn.c_str(), &s);
  return mktime(localtime(&s.st_ctime));
}

#else

static int64_t FILETIME2Unix(FILETIME* t) {
  // https://archive.is/xl8qB
  int64_t time = t->dwLowDateTime | ((int64_t)t->dwHighDateTime << 32), adj;
  adj = 10000 * (int64_t)11644473600000ll;
  time -= adj;
  return time / 10000000ll;
}

int64_t VFsUnixTime(char const* name) {
  std::string fn = VFsFileNameAbs(name);
  if (!FExists(fn))
    return 0;
  FILETIME t;
  HANDLE fh = CreateFileA(fn.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  GetFileTime(fh, NULL, NULL, &t);
  CloseHandle(fh);
  return FILETIME2Unix(&t);
}

#endif

uint64_t VFsFileWrite(char const* name, char const* data, size_t const len) {
  std::string p = VFsFileNameAbs(name);
  if (name) {
    std::ofstream f{p, ios::binary | ios::out};
    if (f)
      f.write(data, len);
  }
  return !!name;
}

void* VFsFileRead(char const* name, uint64_t* const len) {
  if (len)
    *len = 0;
  if (!name)
    return nullptr;
  void* data = nullptr;
  std::string p = VFsFileNameAbs(name);
  if (!FExists(p))
    return nullptr;
  if (FIsDir(p))
    return nullptr;
  std::ifstream f{p, ios::binary | ios::in};
  if (!f)
    return nullptr;
  size_t sz = fs::file_size(p);
  f.read(static_cast<char*>(data = HolyMAlloc(sz + 1)), sz);
  if (len)
    *len = sz;
  static_cast<char*>(data)[sz] = '\0';
  return data;
}

char** VFsDir(char const* fn) {
  std::string file = VFsFileNameAbs("");
  if (!FIsDir(file))
    return nullptr;
  std::vector<char*> items;
  for (auto const& e : fs::directory_iterator{file}) {
    auto const& s = e.path().filename().string();
    // CDIR_FILENAME_LEN is 38(includes '\0')
    // do not touch, fat32 legacy
    // will break opening ISOs if touched
    if (s.size() <= 38 - 1)
      items.emplace_back((char*)HolyStrDup(s.c_str()));
  }
  size_t sz = items.size() * sizeof(char*);
  char** ret;
  std::copy(items.begin(), items.end(),
            ret = static_cast<char**>(HolyMAlloc(sz)));
  return ret;
}

uint64_t VFsIsDir(char const* path) {
  return FIsDir(VFsFileNameAbs(path));
}

uint64_t VFsFileExists(char const* path) {
  return FExists(VFsFileNameAbs(path));
}

void VFsMountDrive(char const let, char const* path) {
  mount_points[toupper(let) - 'A'] = path;
}
