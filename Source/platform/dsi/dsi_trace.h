#pragma once

#ifdef __cplusplus
extern "C" {
#endif
void DsiTraceInit(const char *version);
void DsiTrace(const char *format, ...);
#ifdef __cplusplus
}
#endif
