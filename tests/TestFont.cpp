#include "TestHelper.h"
#include "Font.h"

TEST(TestFont, HeightCalculation) {
  EXPECT_EQ(font::height(1), 7);
  EXPECT_EQ(font::height(2), 14);
  EXPECT_EQ(font::height(3), 21);
  EXPECT_EQ(font::height(4), 28);
}

TEST(TestFont, WidthCalculation) {
  EXPECT_EQ(font::width("", 1), 0);
  EXPECT_EQ(font::width("", 2), 0);
  EXPECT_EQ(font::width("A", 1), 6);
  EXPECT_EQ(font::width("A", 2), 12);
  EXPECT_EQ(font::width("Hello", 1), 30);
  EXPECT_EQ(font::width("Hello", 2), 60);
  EXPECT_EQ(font::width("SCORE: 100", 2), 120);
}

TEST_F(HeadlessSDLTest, DrawExecutionAndSafety) {
  ASSERT_NE(renderer, nullptr);

  // Drawing empty string should succeed without error
  font::draw(renderer, "", 10, 10, 255, 255, 255, 1, 0);

  // Drawing basic ASCII
  font::draw(renderer, "THE COFFEE STAND", 100, 100, 255, 255, 255, 2, 0);

  // Drawing with spaces and punctuation
  font::draw(renderer, "+125 CORRECT! #1", 50, 50, 200, 90, 20, 1, 0);

  // Drawing with maxWidth constraint
  font::draw(renderer, "This is a very long string that should be cut off", 10, 20, 255, 255, 255, 1, 50);

  // Drawing with high-byte characters (c & 0x7F boundary check)
  std::string highByteStr = "Test \x80\xFF\xAA Char";
  font::draw(renderer, highByteStr, 10, 30, 255, 255, 255, 1, 0);
}

TEST_F(HeadlessSDLTest, PixelColorVerification) {
  ASSERT_NE(renderer, nullptr);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  // Draw 'A' in bright red (255, 0, 0) at (20, 20)
  font::draw(renderer, "A", 20, 20, 255, 0, 0, 1, 0);

  // Read back a small 10x10 rect around (20, 20)
  SDL_Rect rect = {20, 20, 10, 10};
  uint32_t pixels[100] = {0};
  int ret = SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGBA8888, pixels, 10 * sizeof(uint32_t));
  EXPECT_EQ(ret, 0);

  // Check that at least one foreground pixel (red) was drawn
  bool foundRed = false;
  for (int i = 0; i < 100; ++i) {
    uint8_t r = (pixels[i] >> 24) & 0xFF;
    uint8_t g = (pixels[i] >> 16) & 0xFF;
    uint8_t b = (pixels[i] >> 8) & 0xFF;
    if (r == 255 && g == 0 && b == 0) {
      foundRed = true;
      break;
    }
  }
  EXPECT_TRUE(foundRed);
}
