U0 KeyDescSet(U8 *fmt,...)
{//Call this from key hndlr to report desc in $LK,"KeyMap",A="MN:KeyMap"$().
  U8 *buf=StrPrintJoin(NULL,fmt,argc,argv);
  StrCpy(keydev.desc,buf);
  keydev.hndlr=Caller;
  Free(buf);
}

U0 KeyDevRem(CKeyDevEntry *tmpk)
{//Remove StdOut hook and free.
  QueRem(tmpk);
  Free(tmpk);
}

CKeyDevEntry *KeyDevAdd(Bool (*fp_put_key)(I64 ch,I64 sc),
	Bool (*fp_puts)(U8 *st),I64 priority,Bool key_descs=FALSE)
{//Places hook in StdOut chain. See $LK,"Keyboard Devices",A="HI:Keyboard Devices/System"$.
  CKeyDevEntry *tmpk=keydev.put_key_head.last,
	*tmpk1=CAlloc(sizeof(CKeyDevEntry),adam_task);
  tmpk1->put_key=fp_put_key;
  tmpk1->put_s=fp_puts;
  tmpk1->priority=priority;
  if (key_descs)
    tmpk1->flags|=KDF_HAS_DESCS;
  while (tmpk->priority>priority)
    tmpk=tmpk->last;
  QueIns(tmpk1,tmpk);
  if (tmpk->priority==priority)
    KeyDevRem(tmpk);
  return tmpk1;
}


Bool KDInputFilterPutKey(I64 ch,I64 scan_code)
{
  if (Bt(&(Fs->task_flags),TASKf_INPUT_FILTER_TASK)) {
    Msg(MSG_KEY_DOWN,ch,scan_code);
    return TRUE;
  } else
    return FALSE;
}

Bool KDInputFilterPutS(U8 *st)
{
  I64 ch;
  if (Bt(&(Fs->task_flags),TASKf_INPUT_FILTER_TASK)) {
    while (ch=*st++)
      Msg(MSG_KEY_DOWN,ch,0);
    return TRUE;
  } else
    return FALSE;
}

U0 CtrlAltDel(I64)
{
  //LBts(sys_ctrl_alt_flags,CTRL_ALT_DEL); TODO URGENT RESTORE
}

U0 CtrlAltC(I64)
{
  U8 *old_rip;
  if(TaskValidate(sys_focus_task)) {
    if(!Bt(&sys_focus_task->task_flags,TASKf_BREAK_LOCKED)) {
      old_rip=sys_focus_task->rip;
      sys_focus_task->rsp-=8;
      sys_focus_task->rsp(U8**)[0]=old_rip;
      sys_focus_task->rip=_BREAK;
    } else
      LBts(&sys_focus_task->task_flags,TASKf_PENDING_BREAK);
  }
}

U0 CtrlAltF(I64)
{
  SwapI64(&text.font,&text.aux_font);
}

U0 CtrlAltM(I64)
{
  //Mute(!IsMute); TODO URGENT RESTORE
}
U0 UCL2(I64) {
  UserCmdLine;
}
U0 CtrlAltT(I64)
{
  Spawn(&UCL2,,,,);
}

U0 CtrlAltX(I64)
{
  if(TaskValidate(sys_focus_task))
	Kill(sys_focus_task);
}
U0 CtrlAltN(I64) {
	if(TaskValidate(sys_focus_task))
		TaskFocusNext;
}

U0 CtrlAltCBSet(U8 ch,U0 (*fp_hndlr)(I64 sc),
	U8 *no_shift_desc=NULL,U8 *shift_desc=NULL,Bool in_irq=FALSE)
{//Set callback for <CTRL-ALT-letter>.
  ch=ToUpper(ch)-'A';
  if (ch<26) {
    keydev.fp_ctrl_alt_cbs[ch]=fp_hndlr;

    Free(keydev.ctrl_alt_no_shift_descs[ch]);
    if (no_shift_desc)
      keydev.ctrl_alt_no_shift_descs[ch]=StrNew(no_shift_desc,adam_task);
    else
      keydev.ctrl_alt_no_shift_descs[ch]=NULL;

    Free(keydev.ctrl_alt_shift_descs[ch]);
    if (shift_desc)
      keydev.ctrl_alt_shift_descs[ch]=StrNew(shift_desc,adam_task);
    else
      keydev.ctrl_alt_shift_descs[ch]=NULL;

    BEqu(&keydev.ctrl_alt_in_irq_flags,ch,in_irq);
  }
}

U0 KeyDevInit()
{
  keydev.fp_ctrl_alt_cbs	=CAlloc(26*sizeof(U8 *));
  keydev.ctrl_alt_no_shift_descs=CAlloc(26*sizeof(U8 *));
  keydev.ctrl_alt_shift_descs	=CAlloc(26*sizeof(U8 *));
  keydev.ctrl_alt_in_irq_flags	=0;
  MemSet(&keydev.put_key_head,0,sizeof(CKeyDevEntry));
  QueInit(&keydev.put_key_head);
  KeyDevAdd(&KDInputFilterPutKey,&KDInputFilterPutS,0x40000000,FALSE);
  KeyDevAdd(&KDDocPutKey,&KDDocPutS,0x80000000,TRUE);
  CtrlAltCBSet('A',&CtrlAltA,"Cmd /Extended ASCII");
  CtrlAltCBSet('G',&CtrlAltG,"Cmd /Grid On","Cmd /Grid Off");
  //CtrlAltCBSet('C',&CtrlAltC,"Cmd /Break Execution",,TRUE); //Handled by input routine
  //CtrlAltCBSet('D',&CtrlAltD,"Cmd /Enter Debugger",,TRUE);
  CtrlAltCBSet('N',&CtrlAltN,"Cmd /Next Window",,TRUE);
  CtrlAltCBSet('F',&CtrlAltF,"Cmd /Toggle Aux Font");
  //CtrlAltCBSet('M',&CtrlAltM,"Cmd /Toggle Mute");
  CtrlAltCBSet('N',&CtrlAltN,"Cmd /Next Focus Task",,TRUE);
  CtrlAltCBSet('T',&CtrlAltT,"Cmd /Terminal Window");
  //CtrlAltCBSet('V',&CtrlAltV,"Cmd /VGA Flush",,TRUE);
  //CtrlAltCBSet('X',&CtrlAltX,"Cmd /Kill Focused Task",,TRUE); Handled by input routine
  CtrlAltCBSet('B',&CtrlAltB,"Cmd /Bible");
  CtrlAltCBSet('Z',&CtrlAltZ,"Cmd /Zoom In",
	"Cmd /Zoom Out");
  //Clipboard supports utf8 
  //CtrlAltCBSet('S',&CtrlAltS,"Cmd /Scrn Shot to Clip");
}
KeyDevInit;
