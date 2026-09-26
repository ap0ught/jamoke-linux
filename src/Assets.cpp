#include "Assets.h"
#include <SDL_image.h>
#include <SDL_audio.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

static std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

Assets::Assets(SDL_Renderer* r) : renderer(r) {}

void Assets::indexDir(const std::string& dir) {
  std::error_code ec;
  // One pass over the whole DATA tree, keying files by lowercased relative
  // path so every later lookup is case-insensitive regardless of how the
  // original media directory happens to be cased.
  for (auto& it : fs::recursive_directory_iterator(dir, ec)) {
    if (!it.is_regular_file()) continue;
    std::string rel = fs::relative(it.path(), root).generic_string();
    if (rel.rfind("/", 0) == 0) rel.erase(0, 1);
    files[lower(rel)] = it.path().string();
  }
}

bool Assets::loadRoot(const std::string& dir) {
  if (!fs::is_directory(dir)) return false;
  root = dir;
  indexDir(dir);
  return !files.empty();
}

void Assets::reloadArt() {
  for (auto& kv : textures) SDL_DestroyTexture(kv.second);
  textures.clear();
}

SDL_Texture* Assets::tex(const std::string& relLower) {
  // Lazy per-texture cache: decoding + uploading on first use keeps the tree
  // index pure filesystem info, and repeat draws stay fast.
  auto hit = textures.find(relLower);
  if (hit != textures.end()) return hit->second;
  auto file = files.find(lower(relLower));
  if (file == files.end()) return nullptr;
  SDL_Surface* surf = IMG_Load(file->second.c_str());
  if (!surf) return nullptr;

  if (surf->pixels && surf->w > 0 && surf->h > 0) {
    Uint8 r = 0, g = 0, b = 0, a = 0;
    Uint32 pixel = 0;
    if (surf->format->BytesPerPixel == 1) {
      pixel = *(const Uint8*)surf->pixels;
    } else if (surf->format->BytesPerPixel == 2) {
      pixel = *(const Uint16*)surf->pixels;
    } else if (surf->format->BytesPerPixel == 3) {
      const Uint8* p = (const Uint8*)surf->pixels;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
      pixel = (p[0] << 16) | (p[1] << 8) | p[2];
#else
      pixel = p[0] | (p[1] << 8) | (p[2] << 16);
#endif
    } else if (surf->format->BytesPerPixel == 4) {
      pixel = *(const Uint32*)surf->pixels;
    }
    SDL_GetRGBA(pixel, surf->format, &r, &g, &b, &a);

    bool isKey = (r > 200 && b > 200 && g < 60) ||  // Magenta (255, 0, 255)
                 (r <= 1 && g <= 1 && b <= 1);       // Black / Dark key (1, 1, 1) or (0, 0, 0)
    std::string pathLower = lower(file->second);
    if (pathLower.size() >= 4 && pathLower.rfind(".bmp") == pathLower.size() - 4) {
      if (isKey) {
        SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format, r, g, b));
      }
    }
  }

  SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, surf);
  SDL_FreeSurface(surf);
  textures[relLower] = t;
  return t;
}

int Assets::texW(const std::string& relLower) {
  SDL_Texture* t = tex(relLower);
  int w = 0, h = 0;
  if (t) SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
  return w;
}

int Assets::texH(const std::string& relLower) {
  SDL_Texture* t = tex(relLower);
  int w = 0, h = 0;
  if (t) SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
  return h;
}

void Assets::play(const std::string& relLower) {
  auto file = files.find(lower(relLower));
  if (file == files.end()) return;

  // Decode the wav on first use and cache the raw PCM; later calls just
  // re-queue the cached buffer (audio files are small, so this is cheap).
  auto lenIt = wavLen.find(relLower);
  if (lenIt == wavLen.end()) {
    SDL_AudioSpec spec;
    Uint8* buf = nullptr;
    Uint32 len = 0;
    SDL_AudioSpec* got = SDL_LoadWAV(file->second.c_str(), &spec, &buf, &len);
    if (!got) return;
    wavSpecs[relLower] = spec;
    wavData[relLower] = buf;
    wavLen[relLower] = len;
  }

  // Open the shared output device on the first sound, letting SDL adjust the
  // format it accepts; every subsequent file is resampled into devSpec below.
  if (!devOpen) {
    dev = SDL_OpenAudioDevice(nullptr, 0, &wavSpecs[relLower], &devSpec,
                              SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (dev == 0) return;
    devOpen = true;
    SDL_PauseAudioDevice(dev, 0);
  }
  // Always route through an audio stream, even when the formats happen to
  // match: it is the only guaranteed path to PCM in the device's actual
  // format (SDL may have altered freq/channels despite the allow flags) and
  // it feeds SDL_QueueAudio in bounded chunks.
  SDL_AudioStream* stream = SDL_NewAudioStream(
      wavSpecs[relLower].format, wavSpecs[relLower].channels, wavSpecs[relLower].freq,
      devSpec.format, devSpec.channels, devSpec.freq);
  if (stream) {
    SDL_AudioStreamPut(stream, wavData[relLower], wavLen[relLower]);
    Uint8 out[65536];
    while (true) {
      int got = SDL_AudioStreamGet(stream, out, sizeof(out));
      if (got <= 0) break;
      SDL_QueueAudio(dev, out, (Uint32)got);
    }
    SDL_FreeAudioStream(stream);
  }
}