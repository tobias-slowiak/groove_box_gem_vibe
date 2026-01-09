#pragma once
//compiel
#ifdef DEBUG_BUILD
#define DEBUG_RT_PRINTF(...) rt_printf(__VA_ARGS__)
#else
#define DEBUG_RT_PRINTF(...) do {} while(0)
#endif

#ifdef DEBUG_BUILD
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...) do {} while(0)
#endif