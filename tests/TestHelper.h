#ifndef JAMOKE_TEST_HELPER_H
#define JAMOKE_TEST_HELPER_H

#include <gtest/gtest.h>
#include <SDL.h>
#include <SDL_image.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "Assets.h"
#include "Drinks.h"
#include "Game.h"

namespace fs = std::filesystem;

inline std::string getAssetRoot() {
  const char* env = std::getenv("JAMOKE_ASSETS");
  if (env && *env) return env;
#ifdef JAMOKE_DEFAULT_ASSETS
  return JAMOKE_DEFAULT_ASSETS;
#else
  return "";
#endif
}

class JamokeTestEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
      std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    }
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
  }

  void TearDown() override {
    IMG_Quit();
    SDL_Quit();
  }
};

class HeadlessSDLTest : public ::testing::Test {
 protected:
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;

  void SetUp() override {
    window = SDL_CreateWindow("JamokeTest", 0, 0, 800, 600, SDL_WINDOW_HIDDEN);
    if (window) {
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
      if (renderer) {
        SDL_RenderSetLogicalSize(renderer, 800, 600);
      }
    }
  }

  void TearDown() override {
    if (renderer) {
      SDL_DestroyRenderer(renderer);
      renderer = nullptr;
    }
    if (window) {
      SDL_DestroyWindow(window);
      window = nullptr;
    }
  }
};

class TempDirGuard {
 public:
  fs::path path;

  explicit TempDirGuard(const std::string& prefix = "jamoke_test_") {
    path = fs::temp_directory_path() / (prefix + std::to_string(std::rand()) + "_" + std::to_string(SDL_GetTicks()));
    fs::create_directories(path);
  }

  ~TempDirGuard() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }

  void writeFile(const std::string& filename, const std::string& content) {
    fs::path p = path / filename;
    if (p.has_parent_path()) {
      fs::create_directories(p.parent_path());
    }
    std::ofstream ofs(p);
    ofs << content;
  }
};

#endif
