//Make Your own Distro by #include-ing this file.

#define STD_DISTRO_DVD_CFG	"T\n0x40000\n$PT$MountIDEAuto\nCT\n\n\n\n"

#define KERNEL_BIN_C	"Kernel.BIN.C"
#define BOOT_DIR	"/0000Boot"
#define BOOT_DIR_KERNEL_BIN_C	BOOT_DIR "/0000" KERNEL_BIN_C


U0 CopyDrv2Drv(U8 drv1,U8 *path1,U8 drv2,U8 *path2) {
  path1=MStrPrint("%C:/%s",drv1,path1);
  path2=MStrPrint("%C:/%s",drv2,path2);
  Copy(path1,path2);
  Free(path1),Free(path2);
}
U0 CopyTreeDrv2Drv(U8 drv1,U8 *path1,U8 drv2,U8 *path2) {
  path1=MStrPrint("%C:/%s",drv1,path1);
  path2=MStrPrint("%C:/%s",drv2,path2);
  CopyTree(path1,path2);
  Free(path1),Free(path2);
}


U0 MakeMyISO(U8 *in_iso,U8 *_out_iso_filename)
{//Does everything with current drive.
//If you have not recompiled $FG,2$Kernel$FG$ and defined your CD/DVD drive, use $LK,"Mount",A="MN:Mount"$.
  U8 drv1,drv2=':',*path;
  U8 *out_iso_filename=FileNameAbs(_out_iso_filename);
  if (!DrvIsWritable) {
    "Drive must be writable.  Install on Hard drive, first.\n";
    return;
  }
  if(!FileExtDot(in_iso)) {
inv_file:
    "MakeMyISO needs an $$BK,1$$.ISO.C$$BK,0$$ File!!!\n";
    return;
  } else if(StrCmp(FileExtDot(in_iso),".ISO.C")) {
    goto inv_file;
  }
  DelTree("/Distro");
  Del(out_iso_filename);

  drv1=MountFile(in_iso);

  DirMk("/Distro");
  In(STD_DISTRO_DVD_CFG);
  path=MStrPrint("%C:/",drv1);
  Cd(path);
  BootDVDIns("::/Distro");
  CopyDrv2Drv(drv1,"/*",':',"/Distro");
  //Del("/Distro/" KERNEL_BIN_C); Why are we delting the bin Terry???

  CopyTreeDrv2Drv(drv1,"/*",':',"/Distro");
  CopyTreeDrv2Drv(drv1,BOOT_DIR,':',"/Distro" BOOT_DIR);
  CopyTreeDrv2Drv(drv1,"/Home",':',"/Distro/Home");
  CopyTreeDrv2Drv(drv1,"/Adam",':',"/Distro/Adam");
  CopyTreeDrv2Drv(drv1,"/Apps",':',"/Distro/Apps");
  CopyTreeDrv2Drv(drv1,"/Compiler",':',"/Distro/Compiler");
  CopyTreeDrv2Drv(drv1,"/Demo",':',"/Distro/Demo");
  CopyTreeDrv2Drv(drv1,"/Doc",':',"/Distro/Doc");
  CopyTreeDrv2Drv(drv1,"/Kernel",':',"/Distro/Kernel");
  CopyTreeDrv2Drv(drv1,"/Misc",':',"/Distro/Misc");

  //To save space, optionally delete dictionary.
  //Del("/Distro/Adam/AutoComplete/ACDefs.DATA");
  CopyTreeDrv2Drv(drv1,"/Downloads",':',"/Distro/Downloads");	  //You can leave this out.
  Cd("::/");
  DirMk("/Distro/Tmp");
  DirMk("/Distro/Tmp/ScrnShots");
  Cd("::/");
  Move("/Distro/" "Kernel.BIN.Z","/Distro" BOOT_DIR_KERNEL_BIN_C);
  Move("/Distro/" "Compiler.BIN.Z","/Distro/Compiler");
  RedSeaISO(out_iso_filename,"T:/Distro",BOOT_DIR_KERNEL_BIN_C);

  //If CD-ROM use MT_CD instead of MT_DVD.
  //DVDImageWrite('T',out_iso_filename,MT_DVD); //Uncomment to burn.

  //DelTree("/Distro");
  Free(out_iso_filename),Free(path);
//  Unmount(drv1);
}

MakeMyISO("/TempleOSLite.ISO.C","/MyDistro.ISO.C");

// Study my account examples $LK,"Cfg Strs",A="FL:::/Demo/AcctExample/TOS/TOSCfg.HC,1"$, $LK,"Update Funs",A="FL:::/Demo/AcctExample/TOS/TOSDistro.HC,1"$.
