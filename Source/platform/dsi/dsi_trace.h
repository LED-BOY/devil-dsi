#pragma once

#ifdef __cplusplus
extern "C" {
#endif
void DsiTraceInit(void);
void DsiTrace(const char *format, ...);
#ifdef __cplusplus
}
#endif
