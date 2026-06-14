#pragma once
#include "render.hpp"

namespace viz {

// Print a RenderFrame to stdout using ANSI escape codes.
// Clears the screen first. Requires an ANSI-capable terminal.
void print_ascii_frame(const RenderFrame& rf);

} // namespace viz
