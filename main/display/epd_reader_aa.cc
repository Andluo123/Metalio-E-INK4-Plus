#include "epd_reader_aa.h"

namespace {
bool s_enabled = false;
}
bool EpdReaderAaEnabled() { return s_enabled; }
void EpdSetReaderAaEnabled(bool enabled) { s_enabled = enabled; }
