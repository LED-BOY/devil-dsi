#ifdef USE_SDL3
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#else
#include <SDL.h>
#include <SDL_main.h>
#endif

#ifdef __SWITCH__
#include "platform/switch/network.h"
#include "platform/switch/random.hpp"
#include "platform/switch/romfs.hpp"
#endif
#ifdef __3DS__
#include "platform/ctr/system.h"
#endif
#ifdef __vita__
#include "platform/vita/network.h"
#include "platform/vita/random.hpp"
#endif
#ifdef NXDK
#include <nxdk/mount.h>
#endif
#ifdef GPERF_HEAP_MAIN
#include <gperftools/heap-profiler.h>
#endif

#include "diablo.h"
#ifdef __DSI__
#include <nds.h>
#include <fat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <vector>
#include "utils/paths.h"
#include "platform/dsi/dsi_trace.h"

namespace {
struct DsiArguments { int argc; char **argv; };
alignas(8) unsigned char DsiEngineStack[128 * 1024];
Thread DsiEngineThread;
int DsiEngineEntry(void *context)
{
	auto *args = static_cast<DsiArguments *>(context);
	DsiTrace("enter DiabloMain");
	return devilution::DiabloMain(args->argc, args->argv);
}
} // namespace
#endif

#if !defined(__APPLE__)
extern "C" const char *__asan_default_options() // NOLINT(bugprone-reserved-identifier, readability-identifier-naming)
{
	return "halt_on_error=0";
}
#endif

extern "C" int main(int argc, char **argv)
{
#ifdef __DSI__
	consoleDemoInit();
	consoleDebugInit(DebugDevice_CONSOLE);
	defaultExceptionHandler();
	setvbuf(stdout, nullptr, _IONBF, 0);
	printf("DevilutionX DSi engine R4\nMenu reader + top-screen map\n");
	if (!isDSiMode() || !fatInitDefault()) {
		printf("DSi mode and SD access required.\nSTART: exit\n");
		while (pmMainLoop()) { swiWaitForVBlank(); scanKeys(); if (keysDown() & KEY_START) break; }
		return 1;
	}
	mkdir("sd:/devilutionx", 0777);
	if (chdir("sd:/devilutionx") != 0) return 1;
	DsiTraceInit();
	DsiTrace("SD mounted, DSi mode confirmed");
	devilution::paths::SetBasePath("sd:/devilutionx/");
	devilution::paths::SetPrefPath("sd:/devilutionx/");
	devilution::paths::SetConfigPath("sd:/devilutionx/");
	devilution::paths::SetAssetsPath("sd:/devilutionx/assets/");
	printf("SD ready. Starting engine...\n");
#endif
#ifdef __SWITCH__
	switch_romfs_init();
	switch_enable_network();
#ifdef PACKET_ENCRYPTION
	randombytes_switchrandom_init();
#endif
#endif
#ifdef __3DS__
	ctr_sys_init();
#endif
#ifdef __vita__
	vita_enable_network();
#ifdef PACKET_ENCRYPTION
	randombytes_vitarandom_init();
#endif
#endif
#ifdef NXDK
	nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
#endif
#ifdef GPERF_HEAP_MAIN
	HeapProfilerStart("main");
#endif
#ifdef __DSI__
	// The default DTCM stack is too small for the engine's loading call chain.
	std::vector<char *> commandLine;
	for (int i = 0; i < argc; ++i) commandLine.push_back(argv[i]);
	if (commandLine.empty()) commandLine.push_back(const_cast<char *>("devilutionx"));
	commandLine.push_back(const_cast<char *>("-n")); // Skip expensive startup movies.
	commandLine.push_back(nullptr);
	DsiArguments args { static_cast<int>(commandLine.size() - 1), commandLine.data() };
	threadPrepare(&DsiEngineThread, DsiEngineEntry, &args,
	    DsiEngineStack + sizeof(DsiEngineStack), MAIN_THREAD_PRIO);
	threadAttachLocalStorage(&DsiEngineThread, nullptr);
	threadStart(&DsiEngineThread);
	const int result = threadJoin(&DsiEngineThread);
#else
	const int result = devilution::DiabloMain(argc, argv);
#endif
#ifdef GPERF_HEAP_MAIN
	HeapProfilerStop();
#endif
	return result;
}
