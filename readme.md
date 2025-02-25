# templeos in ring 3
### you ***must*** already know the basics of templeos before attempting to use this
"tos" in lowercase in this project's codebase and Wiki does not refer to TempleOS' abbreviation, it refers to this project(runtime/loader) <br>
it's hard to describe what this exactly is because it doesn't "emulate" anything in the traditional sense, though it loads a mostly orthodox kernel and can compile/run HolyC software as you would on a real bare metal machine as it runs Terry's compiler and emits machine code that's executed directly on the host CPU

# building
## windows users
### only supports >=Win10(complain to msys2 devs not me), msvc unsupported
install msys2, launch the "MSYS2 MINGW64 Shell", and run the following
```
pacman -Syu make yasm mingw-w64-x86_64-{gcc,SDL2,cmake,libuv}
```
if pacman tells you to restart the terminal then do it and run the cmd again(rtfm)
## unix-like system users
install SDL2, cmake, make, yasm, gcc/clang and libuv
## building the loader
```
mkdir build;cd build;
cmake ..; # *nix
cmake .. -G 'MSYS Makefiles' # win32, -G flag very important
make -j$(nproc);
```
# build runtime
```
./tos -f HCRT_BOOTSTRAP.BIN -ctT BuildHCRT.HC
mv T/HCRT.BIN .
```
# run
```
./tos -t T #-h for info on other flags
```
# caveats
due to running in userspace, context switching is around 4 times slower(not that it matters anyway outside of flexing `CPURep(TRUE);` results) and ring 0 routines like In/OutU8 are not present <br>
division by zero is not an exception, it will bring up the debugger(SIGFPE)

# documentation
```C
Cd("T:/Server");
#include "run";
//point browser to localhost:8080
```
contributions to wiki appreciated

# building TempleOS from tos
because this runtime uses the orthodox version of the HolyC compiler,building a TempleOS distro is possible(why) <br>
ISO must end in `ISO.C`

```
//Download an orthodox ISO into your T drive from TempleOS.org
Move("TempleOSLite.ISO","TempleOSLite.ISO.C"); //Move it to end in ISO.C
#include "Boot/DoDistro.HC";
MakeMyISO("/TempleOSLite.ISO.C","/MyDistro.ISO.C");
//ExitTOS; optionally exit tos idk
//Run the ISO using qemu -m 512M -cdrom T/MyDistro.ISO.C
```

# ref
```C
DirMk("folder");
Dir;
Dir("fold*");//supports wildcards
Cd("folder");
Man("Ed");
Ed("file.HC.Z");
Unzip("file.HC.Z");//unzip tos compression
Zip("file.HC");
Find("str",,"-i");//grep -rn . -e str
FF("file.*");//find .|grep file
MountFile("Something.ISO.C");//MountFile(U8 *f,U8 drv_let='M');
Cd("M:/");//defaults to M
INT3;//force raise debug situation
DbgHelp;//help on how to debug
ExitTOS(I32i ec=0);
```
