#include <gtest/gtest.h>

#include "Input.h"

// The window->logical translation is the one link that can silently mis-route
// every click in the game, and it has no other coverage: handleClick is fed
// logical coordinates, so a mistake here shows up as "some controls don't
// work" rather than an error. These tests pin the math at the window sizes
// that matter, including the degenerate queries a failed SDL call can return.

TEST(Input, IdentityWhenWindowMatchesLogicalSize) {
  EXPECT_EQ(input::toLogical(400, 800, 800), 400);
  EXPECT_EQ(input::toLogical(400, 600, 600), 400);
  EXPECT_EQ(input::toLogical(0, 800, 800), 0);
  EXPECT_EQ(input::toLogical(799, 800, 800), 799);
}

TEST(Input, ScalesUpAndDown) {
  // Window larger than the logical space (resize, maximize, HiDPI).
  EXPECT_EQ(input::toLogical(512, 800, 1024), 400);
  EXPECT_EQ(input::toLogical(640, 600, 768), 500);
  // Window smaller than the logical space.
  EXPECT_EQ(input::toLogical(320, 800, 640), 400);
  EXPECT_EQ(input::toLogical(240, 600, 480), 300);
}

TEST(Input, DegenerateSizesFallBackToIdentity) {
  // A failed SDL_GetWindowSize must not collapse every click to (0,0).
  EXPECT_EQ(input::toLogical(123, 800, 0), 123);
  EXPECT_EQ(input::toLogical(123, 0, 800), 123);
  EXPECT_EQ(input::toLogical(123, 0, 0), 123);
  EXPECT_EQ(input::toLogical(123, -8, 800), 123);
}

TEST(Input, RoundTripsThroughWindowSpace) {
  // The --script play mode aims with toWindow and the app maps back with
  // toLogical, so a lossy round trip would make replays untrustworthy.
  const int sizes[][2] = {{800, 600}, {1024, 768}, {640, 480}, {1600, 1200}};
  for (const auto& s : sizes) {
    for (int lx = 0; lx <= 800; lx += 200) {
      for (int ly = 0; ly <= 600; ly += 200) {
        int wx = input::toWindow(lx, 800, s[0]);
        int wy = input::toWindow(ly, 600, s[1]);
        EXPECT_EQ(input::toLogical(wx, 800, s[0]), lx)
            << "x round trip failed at " << lx << " for " << s[0];
        EXPECT_EQ(input::toLogical(wy, 600, s[1]), ly)
            << "y round trip failed at " << ly << " for " << s[1];
      }
    }
  }
}
