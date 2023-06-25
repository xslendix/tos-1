#pragma once

#include <stddef.h>
#include <stdint.h>

void SetClipboard(char const* text);
char* ClipboardText();
struct CDrawWindow;
CDrawWindow* NewDrawWindow();
void DrawWindowUpdate(CDrawWindow* w, int8_t* colors, int64_t internal_width,
                      int64_t h);
void InputLoop(bool* off);
void GrPaletteColorSet(uint64_t, uint64_t);

void SetKBCallback(void* fp, void* data);
void SetMSCallback(void* fp);
