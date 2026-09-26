#ifndef JAMOKE_INPUT_H
#define JAMOKE_INPUT_H

// Coordinate math for the fixed 800x600 logical space.
//
// This is deliberately free of SDL types and global state: the main loop maps
// real mouse events through it, the scripted play mode (--script) pushes
// synthetic events back through the *same* mapping, and the unit tests check
// it directly. Before this existed the translation lived inline in the event
// loop, which meant the one link that can silently mis-route every click in
// the game had no test at all and could not be driven without a human mouse.

namespace input {

// Window pixels -> logical space, mirroring SDL_RenderSetLogicalSize scaling.
// Degrades to the identity when either size is unknown, so a failed query can
// never turn every click into (0,0).
inline int toLogical(int windowCoord, int logicalSize, int windowSize) {
  if (logicalSize <= 0 || windowSize <= 0) return windowCoord;
  return (int)(windowCoord * ((float)logicalSize / windowSize));
}

// Logical space -> window pixels. The inverse, used by the script player to
// aim a synthetic click at a logical position: it deliberately travels back
// through the app's own toLogical() so the round trip exercises the real
// translation instead of bypassing it.
inline int toWindow(int logicalCoord, int logicalSize, int windowSize) {
  if (logicalSize <= 0 || windowSize <= 0) return logicalCoord;
  return (int)(logicalCoord * ((float)windowSize / logicalSize));
}

}  // namespace input

#endif
