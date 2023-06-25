#pragma once

#include <stddef.h>
#include <stdint.h>

void* NewVirtualChunk(size_t sz, bool low32);
void FreeVirtualChunk(void* ptr, size_t s);
