#include "dsi_trace.h"
#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>

static int ready;
static unsigned lines;

void DsiTraceInit(void)
{
    FILE *file = fopen("sd:/devilutionx/dsi-startup.txt", "w");
    if (!file) {
        printf("Cannot create dsi-startup.txt\n");
        return;
    }
    fputs("DSi engine R6 test6: scene resolution + map toggle\n"
          "heap_used/heap_free_in_arena/arena_bytes describe malloc's arena,\n"
          "not all free DSi RAM. File is reset on each launch.\n", file);
    fclose(file);
    ready = 1;
    lines = 0;
}

void DsiTrace(const char *format, ...)
{
    if (!ready || lines >= 256) return;
    char message[192];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    struct mallinfo memory = mallinfo();
    FILE *file = fopen("sd:/devilutionx/dsi-startup.txt", "a");
    if (file) {
        fprintf(file, "%03u %s | heap_used=%lu heap_free_in_arena=%lu arena_bytes=%lu\n",
            lines, message, (unsigned long)memory.uordblks,
            (unsigned long)memory.fordblks, (unsigned long)memory.arena);
        fclose(file);
    }
    ++lines;
    printf("DSi: %s\n", message);
}
