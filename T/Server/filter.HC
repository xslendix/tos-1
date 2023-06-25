class CTrie {
	U8 *word;
	CTrie *branches['Z'-'A'+1+1];
};
U0 TrieIns(CTrie *parent,U8 *word,U8 *_word) {
	U8 chr=ToUpper(*_word);
	if(!chr) {
		Free(parent->word);
		parent->word=StrNew(word);
		return;
	}
	switch(chr) {
		case 'A'...'Z':
		aloop:
		if(parent->branches[chr-'A']) {
			TrieIns(parent->branches[chr-'A'],word,_word+1);
		} else  {
			parent->branches[chr-'A']=CAlloc(sizeof(CTrie));
			goto aloop;
		}
	}
}
CTrie *TrieGet(CTrie *parent,U8 *_word) {
	U8 chr=ToUpper(*_word);
	if(!parent) return NULL;
	if(!*_word) {
		if(parent->word)
			return parent;
		return NULL;
	}
	switch(chr) {
		case 'A'...'Z':
		return TrieGet(parent->branches[chr-'A'],_word+1);
	}
	return NULL;
}
CTrie *ConstructProfans() {
	CTrie *profans=CAlloc(sizeof(CTrie));
	U8 *ptr,*ftxt,*ln,*oln;
	ptr=ftxt=FileRead("bad-words.txt");
	while(ln=ReadLine(ptr,&ptr)) {
		if(StrFirstOcc(ln,"\n"))
			*StrFirstOcc(ln,"\n")=0;
		StrUtil(ln,SUF_REM_SPACES|SUF_REM_CTRL_CHARS);
		TrieIns(profans,ln,ln);
		Free(ln);
	}
	Free(ftxt);
	return profans;
}
CTrie *profans=ConstructProfans;
Bool IsLetter(U8 chr) {
	switch(chr) {
		case 'a'...'z':
		case 'A'...'Z':
		return TRUE;
	}
	return FALSE;
}
U8 *HasProfanity(U8 *text) {
	I64 i=0,bi;
	CTrie *t;
	U8 buffer[STR_LEN];
	loop:
	if(IsLetter(text[i])) {
		if(!i||!IsLetter(text[i-1])) {
			for(bi=0;IsLetter(text[i]);)
				buffer[bi++]=text[i++];
			buffer[bi]=0;
			if(t=TrieGet(profans,buffer))
				return StrNew(t->word);
			goto loop;
		}
	}
	if(text[i++])
		goto loop;
	return NULL;
}