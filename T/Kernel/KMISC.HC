F64 tS()
{//Time since boot in seconds as a float.
  return __GetTicks/1000.;
}
Bool Blink(F64 Hz=2.5)
{//Return TRUE, then FALSE, then TRUE at given frequency.
  if (!Hz) return 0;
  return ToI64(__GetTicks*2*Hz/1000.)&1;
}

U16 EndianU16(U16 d)
{//Swap big<-->little endian.
  I64 res=0;
  res.u8[1]=d.u8[0];
  res.u8[0]=d.u8[1];
  return res;
}

U32 EndianU32(U32 d)
{//Swap big<-->little endian.
  I64 res=0;
  res.u8[3]=d.u8[0];
  res.u8[2]=d.u8[1];
  res.u8[1]=d.u8[2];
  res.u8[0]=d.u8[3];
  return res;
}

I64 EndianI64(I64 d)
{//Swap big<-->little endian.
  U64 res;
  res.u8[7]=d.u8[0];
  res.u8[6]=d.u8[1];
  res.u8[5]=d.u8[2];
  res.u8[4]=d.u8[3];
  res.u8[3]=d.u8[4];
  res.u8[2]=d.u8[5];
  res.u8[1]=d.u8[6];
  res.u8[0]=d.u8[7];
  return res;
}
F64 Ona2Freq(I8 ona)
{//Ona to freq. Ona=60 is 440.0Hz.
  if (!ona)
    return 0;
  else
    return 440.0/32*2.0`(ona/12.0);
}

I8 Freq2Ona(F64 freq)
{//Freq to Ona. 440.0Hz is Ona=60.
  if (freq>0)
    return ClampI64(12*Log2(32.0/440.0*freq),1,I8_MAX);
  else
    return 0;
}

U0 Snd(I8 ona=0) {
    SndFreq(ToI64(Ona2Freq(ona)));
}

Bool Silent(Bool val=ON)
{//Turn-off StdOut console text. (Not sound.)
  return LBEqu(&(Fs->display_flags),DISPLAYf_SILENT,val);
}

Bool IsSilent()
{//Return StdOut turned-off?
  return Bt(&(Fs->display_flags),DISPLAYf_SILENT);
}
Bool ScrnCast(Bool val=ON,Bool just_audio=FALSE,U8 *print_fmt="B:/Tmp/%X.GR")
{//WinMgr saves GR files to a dir.
  Bool old_val;
  scrncast.just_audio=just_audio;
  if (val) {
    if (!(old_val=LBtr(&scrncast.record,0))) {
      Free(scrncast.print_fmt);
      scrncast.print_fmt=AStrNew(print_fmt);
      scrncast.t0_now=Now;
      scrncast.snd_head.tS=scrncast.t0_tS=tS;
      scrncast.snd_head.ona=scrncast.ona;
      LBts(&scrncast.record,0);
    }
  } else
    old_val=LBtr(&scrncast.record,0);
  Snd;
  return old_val;
}
U0 ProgressBarsRst(U8 *path=NULL)
{//Reset all progress bars to zero.
  CallExtStr("ProgressBarsRegTf",path);
  MemSet(sys_progresses,0,sizeof(sys_progresses));
}
