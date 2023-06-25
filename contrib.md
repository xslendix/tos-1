# ffi
runtime.c
```C
sizeof(each arg)==sizeof(uint64_t)//use ptrs
FFI_CALL_TOS_?(uint64 (*)(...),uint64_t,...);
STK_RegisterFunctionPtr(&ffi_blob,"<tos func name>",uint64_t (*)(Ts...args),sizeof...(Ts)/*important or will segv*/);
```
T/KERNELA.HH
```C
...after #ifdef IMPORT_BUILTINS
import U64i f(....);
...#else then lots of extern
extern <same function prototype>;
//F64 -> ok
//U64 -> bad, use U64i
```
make again with cmake
# extending the kernel
T/KERNELA.HH
```C
//same as ffi
```
T/HCRT\_TOS.HC
```C
#include "<desired holyc file>"
```
# header generation
T/FULL\_PACKAGE.HC
```C
#define GEN_HEADERS 1
```
make -> run tos -> T/unfound.DD
```
<functions>
```
copy desired fn prototypes to T/KERNELA.HH
