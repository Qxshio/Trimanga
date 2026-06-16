#include "review_internal.hpp"

#include "trimanga/image_loader.hpp"

#include <algorithm>

namespace trimanga::preview {

SDL_Texture* load_texture(SDL_Renderer* renderer, const std::filesystem::path& path, int& width, int& height) {
  ColorImage image = load_color_image(path);
  if (!image.valid()) {
    width = 0;
    height = 0;
    return nullptr;
  }
  SDL_Texture* texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, image.width, image.height);
  if (texture == nullptr) {
    width = 0;
    height = 0;
    return nullptr;
  }
  SDL_UpdateTexture(texture, nullptr, image.pixels.data(), image.width * static_cast<int>(sizeof(std::uint32_t)));
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  width = image.width;
  height = image.height;
  return texture;
}

TextureCache::TextureCache(SDL_Renderer* renderer, std::vector<Candidate>& candidates)
    : renderer_(renderer), candidates_(candidates), entries_(candidates.size()) {}

TextureCache::~TextureCache() {
  clear();
}

void TextureCache::clear() {
  for (TextureEntry& entry : entries_) {
    if (entry.texture != nullptr) {
      SDL_DestroyTexture(entry.texture);
      entry = {};
    }
  }
}

TextureEntry& TextureCache::get(std::size_t index) {
  TextureEntry& entry = entries_[index];
  if (entry.texture == nullptr) {
    entry.texture = load_texture(renderer_, candidates_[index].page.image_path, entry.width, entry.height);
  }
  return entry;
}

TextureEntry* TextureCache::peek(std::size_t index) {
  if (index >= entries_.size() || entries_[index].texture == nullptr) {
    return nullptr;
  }
  return &entries_[index];
}

void TextureCache::preload_near(std::size_t center, std::size_t radius) {
  const std::size_t start = center > radius ? center - radius : 0;
  const std::size_t end = std::min(entries_.size(), center + radius + 1);
  for (std::size_t index = start; index < end; ++index) {
    get(index);
  }
  keep_near(center, radius + 3);
}

void TextureCache::keep_near(std::size_t center, std::size_t radius) {
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    const std::size_t distance = index > center ? index - center : center - index;
    if (distance <= radius || entries_[index].texture == nullptr) {
      continue;
    }
    SDL_DestroyTexture(entries_[index].texture);
    entries_[index] = {};
  }
}

}  // namespace trimanga::preview
