#ifndef OPENCMR2_DIAGNOSTICS_SAMPLER_H
#define OPENCMR2_DIAGNOSTICS_SAMPLER_H
namespace Diagnostics {
// OPENCMR2_SAMPLE=path: a poor man's profiler for targets without perf. The program counter is taken
// 1000 times per second of CPU time; the counters go to <path>, with the memory map in <path>.maps, to be
// matched with `nm` (tools/sample_summary.py). Written every few seconds by SamplerTick() and at the end.
void StartSamplerFromEnvironment();
void SamplerTick();
}
#endif
