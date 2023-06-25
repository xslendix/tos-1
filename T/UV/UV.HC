U0 cb(CUVRandom*,I32 status,U8 *buf,U64 sz) {
  if (status) {
    ST_ERR_ST "SOMETHING WENT WRONG\n";
    return;
  }
  U64 i;
  for (i=0;i<sz/8;++i)
    "%lu\n",buf(U64*)[i];
  '\n';
}

U0 Main() {
  U8 buf[1024];
  CUVLoop   *l=UVLoopNew;
  CUVRandom *r=UVRandomNew;
  UVRandom(l,r,buf,1024,,&cb);
  UVRun(l,#exe{GetUVLoopMode(
                 "UV_RUN_DEFAULT");});
  UVRandomDel(r);
  UVLoopDel(l);
}Main;