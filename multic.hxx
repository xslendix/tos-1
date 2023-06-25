#pragma once
// am i bad, am i bad, am i really that bad?
// if you were in my shoes you'll walk the same damn miles i'll do
// (who knows, he will know. he might stumble upon this someday)

#include <stddef.h>
#include <stdint.h>

uint64_t GetTicks();

void* GetFs();
void SetFs(void*);

void* GetGs();
void SetGs(void*);

size_t CoreNum();

void InterruptCore(size_t core);

// https://archive.md/nKvoK
typedef void* VoidCallback(void*);
void LaunchCore0(VoidCallback* fp);
void WaitForCore0();

void CreateCore(size_t core, void* fp);
void ShutdownCore(size_t core);
void ShutdownCores(int ec);

void AwakeFromSleeping(size_t core);

void SleepHP(uint64_t us);
