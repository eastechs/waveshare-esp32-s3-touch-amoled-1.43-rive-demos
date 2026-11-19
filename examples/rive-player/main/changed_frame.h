#pragma once
#include <stdint.h>

// Compare two full half-resolution images, transfer changed columns and update previous.
void display_changes(uint32_t *previous, const uint32_t *frame);
