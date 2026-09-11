// The center-button menu. Left/right change the selected value, center steps to the next
// entry, holding center leaves. Entries apply immediately and are persisted through settings.
#pragma once
#include <Arduino.h>

#include "settings.h"

namespace menu {
void begin(Settings* s);
void open();
void close();
bool isOpen();
void onLeft();
void onRight();
void onCenter();
size_t count();
size_t selected();
void label(size_t i, char* out, size_t n);
void value(size_t i, char* out, size_t n);
bool editable(size_t i);
}
