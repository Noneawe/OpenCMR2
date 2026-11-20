#include "diagnostics/sampler.h"
#include <SDL3/SDL.h>
#include <csignal>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#if defined(__linux__) && (defined(__aarch64__) || defined(__x86_64__))
#include <sys/time.h>
#include <ucontext.h>
#define OPENCMR2_SAMPLER 1
#endif

namespace Diagnostics {
#ifdef OPENCMR2_SAMPLER
static const char *samplePath;
static const size_t capacity = 1 << 20;   // samples of two words: where it was, and who called (aarch64: the link register)
static uintptr_t *samples;
static volatile size_t used;
static uint64_t lastWrite;

static void OnTimer(int, siginfo_t *, void *context)
{
    const ucontext_t *uc = (const ucontext_t *)context;
#if defined(__aarch64__)
    const uintptr_t pc = (uintptr_t)uc->uc_mcontext.pc, caller = (uintptr_t)uc->uc_mcontext.regs[30];
#else
    const uintptr_t pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP], caller = 0;
#endif
    if (used < capacity) {
        samples[used * 2] = pc;
        samples[used * 2 + 1] = caller;
        used++;
    }
}

static void Write()
{
    if (!samplePath)
        return;
    if (FILE *file = fopen(samplePath, "wb")) {
        fwrite(samples, 2 * sizeof(uintptr_t), used, file);
        fclose(file);
    }
    const std::string maps = std::string(samplePath) + ".maps";
    if (FILE *in = fopen("/proc/self/maps", "rb")) {
        if (FILE *out = fopen(maps.c_str(), "wb")) {
            char buffer[4096];
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
                fwrite(buffer, 1, n, out);
            fclose(out);
        }
        fclose(in);
    }
}

void StartSamplerFromEnvironment()
{
    samplePath = SDL_getenv("OPENCMR2_SAMPLE");
    if (!samplePath || !*samplePath || samples) {
        samplePath = samples ? samplePath : nullptr;
        return;
    }
    samples = (uintptr_t *)malloc(capacity * 2 * sizeof(uintptr_t));
    struct sigaction action = {};
    action.sa_sigaction = OnTimer;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &action, nullptr);
    const itimerval interval = { { 0, 1000 }, { 0, 1000 } };
    setitimer(ITIMER_PROF, &interval, nullptr);
    atexit(Write);
    fprintf(stderr, "Sampling profile: %s\n", samplePath);
}

void SamplerTick()
{
    if (!samplePath)
        return;
    const uint64_t now = SDL_GetTicks();
    if (now - lastWrite > 10000) {
        lastWrite = now;
        Write();
    }
}
#else
void StartSamplerFromEnvironment() {}
void SamplerTick() {}
#endif
}
