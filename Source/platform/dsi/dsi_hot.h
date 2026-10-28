#ifndef DEVILUTIONX_DSI_HOT_H
#define DEVILUTIONX_DSI_HOT_H
/* Calico's standard linker/startup loads .itcm; do not remap memory manually.
 * The CMake file compiles these kernels as ARM/O2. Host tests use plain C. */
#if defined(ARM9) && defined(__arm__)
#define DSI_HOT __attribute__((section(".itcm"), long_call, noinline))
#else
#define DSI_HOT
#endif
#endif
