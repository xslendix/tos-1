U0 LinkedLstDel(U8 **_lst)
{   //Free entire linked-list.
    U8 **tmpl;
    while (_lst) {
        tmpl=*_lst;
        Free(_lst);
        _lst=tmpl;
    }
}

U8 *LinkedLstCopy(U8 **_lst,CTask *mem_task=NULL)
{   //MAlloc ident copy of entire linked-list.
    U8 *res=NULL,**tmpl;
    tmpl=&res;
    while (_lst) {
        tmpl=*tmpl=MAllocIdent(_lst);
        _lst=*_lst;
    }
    return res;
}

I64 LinkedLstCnt(U8 **_lst)
{   //Count of nodes in linked-list.
    I64 res=0;
    while (_lst) {
        res++;
        _lst=*_lst;
    }
    return res;
}

I64 LinkedLstSize(U8 **_lst)
{   //Mem size of all nodes in linked-list.
    I64 res=0;
    while (_lst) {
        res+=MSize(_lst);
        _lst=*_lst;
    }
    return res;
}

