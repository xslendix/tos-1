//See $LK,"Install Documentation",A="FI:::/Doc/Install.DD"$.
//Study my account examples: $LK,"Cfg Strs",A="FL:::/Demo/AcctExample/TOS/TOSCfg.HC,1"$, $LK,"Update Funs",A="FL:::/Demo/AcctExample/TOS/TOSDistro.HC,1"$

#include "BootDVD"
#include "DskISORedSea"

#help_index "Install"

#define KERNEL_BIN_C	"Kernel.BIN.C"
#define BOOT_DIR	"/0000Boot"
#define BOOT_DIR_KERNEL_BIN_C	BOOT_DIR "/0000" KERNEL_BIN_C

U0 MakeAll(U8 *dst_loc)
{
  U8 *tmp=MStrPrint("%s/Compiler.BIN.Z",dst_loc);
  if (Cmp("/Compiler/Compiler",,tmp,'T'))
    throw;
  Free(tmp);
  tmp=MStrPrint("%s/Kernel.BIN.Z",dst_loc);
  if (Cmp("/Kernel/Kernel",,tmp,'T'))
    throw;
  Free(tmp);
}

public U0 BootDVDIns(U8 *dst_loc,U8 drv_let=0)
{//See $LK,"::/Misc/DoDistro.HC"$.
  try {
    MakeAll(dst_loc);
//Move(tmp,BOOT_DIR_KERNEL_BIN_C);I will move this later in DoDistro.HC
  } catch {
    PutExcept;
    Beep;
  }
}
