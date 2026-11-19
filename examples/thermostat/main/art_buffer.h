#pragma once
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

static inline void *art_buffer(size_t bytes)
{
#ifdef ESP_PLATFORM
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
#else
    return malloc(bytes);
#endif
}
