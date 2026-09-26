#ifndef JAMOKE_ASSETS_H
#define JAMOKE_ASSETS_H

#include <SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

// Case-insensitive loader for the original Jamoke DATA tree.
// All lookups are keyed by lowercase relative path (e.g. "uiart/background.jpg").
struct Assets {
  explicit Assets(SDL_Renderer* r);

  // Indexes the tree under `root` once. Returns true on success.
  bool loadRoot(const std::string& root);

  // Returns a texture or nullptr if not found.
  SDL_Texture* tex(const std::string& relLower);
  int texW(const std::string& relLower);
  int texH(const std::string& relLower);

  // Audio: loads the wav file on first use and queues it on a shared device.
  void play(const std::string& relLower);

  // Drops the texture cache so the next draw re-decodes every image from
  // disk (the original game's F2 "reload all the art").
  void reloadArt();

  std::string root;
  SDL_Renderer* renderer = nullptr;

  // lowercase rel path -> absolute path (index built in loadRoot)
  std::unordered_map<std::string, std::string> files;
  std::unordered_map<std::string, SDL_Texture*> textures;

  // One shared device plays every sound; wavs are cached decoded here and
  // resampled to devSpec's format on each play() call.
  std::unordered_map<std::string, SDL_AudioSpec> wavSpecs;
  std::unordered_map<std::string, Uint8*> wavData;
  std::unordered_map<std::string, Uint32> wavLen;
  SDL_AudioDeviceID dev = 0;
  SDL_AudioSpec devSpec{};
  bool devOpen = false;

private:
  void indexDir(const std::string& dir);
};

#endif