# templeos in ring 3
### you should already know the basics of templeos before attempting to use this

# building
## windows users
### support only for >=Win10, msvc unsupported
install msys2, launch the "MSYS2 MINGW64 Shell", and run the following
```
pacman -Syu make yasm mingw-w64-x86_64-{gcc,SDL2,cmake,libuv}
```
if pacman tells you to restart the terminal then do it and run the cmd again(rtfm)
## unix-like system users
install SDL2, cmake, make, yasm, gcc/clang(clang preferred) and libuv
## building the loader
```
mkdir build;cd build;
cmake ..; # *nix
cmake .. -G 'MSYS Makefiles' # win32, -G flag very important
make -j$(nproc);
```
# build runtime
```
cp HCRT_BOOTSTRAP.BIN HCRT.BIN
./tos -ctT BuildHCRT.BIN
mv T/HCRT.BIN .
```
# run
```
./tos -t T #-h for info on other flags
```
# caveat
due to running in userspace, context switching is around 4 times slower <br>
division by zero is not an exception, it will bring up the debugger(SIGFPE)

# documentation
```C
Cd("T:/Server");
#include "run";
//point browser to localhost:8080
```
contributions to wiki appreciated

# ref
```C
DirMk("folder");
Dir;
Dir("fold*");//supports wildcards
Cd("folder");
Man("Ed");
Ed("a.HC");
Find("str",,"-i");//grep -rn . -e str
FF("file.*");//find .|grep file
Unzip("file.HC.Z");//unzip tos compression
Zip("file.HC");
DbgHelp;//help on how to debug
INT3;//force raise debug situation
ExitTOS(I32i ec=0);
```
