#pragma once
#include <stdbool.h>
#include <stdint.h>

// Compare two full half-resolution images, transfer changed columns and update previous.
bool display_changes(uint32_t *previous, const uint32_t *frame);
