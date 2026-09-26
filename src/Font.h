#ifndef JAMOKE_FONT_H
#define JAMOKE_FONT_H

#include <SDL.h>
#include <string>

// Tiny built-in 5x7 bitmap font; renders colored text with a dark outline.
namespace font {
void draw(SDL_Renderer* r, const std::string& s, int x, int y,
          Uint8 r8, Uint8 g8, Uint8 b8, int scale = 2, int maxWidth = 0);
int width(const std::string& s, int scale = 2);
int height(int scale = 2);
}

#endif