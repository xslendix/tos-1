# ffi
runtime.cxx
```C
uint64_t STK_FunctionName(uint64_t* stk) {
  // ...
}
S_(FunctionName, function arg cnt);
```
T/KERNELA.HH
```C
...after #ifdef IMPORT_BUILTINS
import U64i FunctionName(....);
...#else then lots of extern
extern <same function prototype>;
//F64 -> ok
//U64 -> bad, use U64i
```
build hcrt and loader again
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
