#pragma once
#include "render.hpp"

namespace viz {

// Initialize ncurses, set up color pairs, enter cbreak/noecho mode.
// Call once before any draw_ncurses_frame() calls.
void init_ncurses();

// Redraw the terminal with the current RenderFrame.
// Must be called between init_ncurses() and cleanup_ncurses().
void draw_ncurses_frame(const RenderFrame& rf);

// Restore terminal to its original state. Call on exit (or before printing errors).
void cleanup_ncurses();

} // namespace viz
