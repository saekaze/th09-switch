#pragma once
#include "Types.hpp"
namespace th09 {
// Windows virtual-key numbers are an input interchange format only. The SDL
// adapter maps physical keys into this snapshot; no Win32 calls are required.
u16 keyboard_input(const bool (&keys)[256],u32 device);
}
