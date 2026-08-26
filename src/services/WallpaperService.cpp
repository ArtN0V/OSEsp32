#include "WallpaperService.h"

#include <algorithm>
#include <cstring>
#include <draw/lv_image_decoder_private.h>
#include <misc/cache/instance/lv_image_cache.h>

namespace {
constexpr char MAGIC[4] = {'O', 'W', 'P', '1'};
constexpr uint16_t FORMAT_RGB565 = 1;
constexpr const char* TEMP_SD_PATH = "/OSEsp32/Wallpapers/desktop.tmp";
constexpr const char* TEMP_LVGL_PATH = "S:/OSEsp32/Wallpapers/desktop.tmp";

bool writeExact(lv_fs_file_t& file, const void* data, uint32_t size) {
  uint32_t written = 0;
  return lv_fs_write(&file, data, size, &written) == LV_FS_RES_OK &&
         written == size;
}
}  // namespace

WallpaperService* WallpaperService::active_ = nullptr;

void WallpaperService::begin(StorageService& storage, Logger& logger) {
  active_ = this;
  storage_ = &storage;
  logger_ = &logger;
  invalidateCache();
  decoder_ = lv_image_decoder_create();
  lv_image_decoder_set_info_cb(decoder_, decoderInfo);
  lv_image_decoder_set_open_cb(decoder_, decoderOpen);
  lv_image_decoder_set_get_area_cb(decoder_, decoderGetArea);
  lv_image_decoder_set_close_cb(decoder_, decoderClose);
}

bool WallpaperService::validHeader(const Header& header) {
  return memcmp(header.magic, MAGIC, sizeof(MAGIC)) == 0 &&
         header.width == WIDTH && header.height == HEIGHT &&
         header.stripLines == STRIP_LINES &&
         header.colorFormat == FORMAT_RGB565 &&
         header.dataSize == static_cast<uint32_t>(WIDTH) * HEIGHT * 2;
}

uint16_t WallpaperService::pixelToRgb565(const uint8_t* pixel,
                                         lv_color_format_t format,
                                         uint16_t background) {
  if (format == LV_COLOR_FORMAT_RGB565)
  {
    uint16_t value;
    memcpy(&value, pixel, sizeof(value));
    return value;
  }
  if (format == LV_COLOR_FORMAT_RGB565_SWAPPED) {
    uint16_t value;
    memcpy(&value, pixel, sizeof(value));
    return static_cast<uint16_t>((value << 8) | (value >> 8));
  }
  if (format == LV_COLOR_FORMAT_RGB888 ||
      format == LV_COLOR_FORMAT_ARGB8888) {
    uint8_t blue = pixel[0];
    uint8_t green = pixel[1];
    uint8_t red = pixel[2];
    if (format == LV_COLOR_FORMAT_ARGB8888 && pixel[3] < 255) {
      const uint8_t alpha = pixel[3];
      const uint8_t bgRed = static_cast<uint8_t>(((background >> 11) & 31) << 3);
      const uint8_t bgGreen = static_cast<uint8_t>(((background >> 5) & 63) << 2);
      const uint8_t bgBlue = static_cast<uint8_t>((background & 31) << 3);
      red = static_cast<uint8_t>((red * alpha + bgRed * (255 - alpha)) / 255);
      green = static_cast<uint8_t>((green * alpha + bgGreen * (255 - alpha)) / 255);
      blue = static_cast<uint8_t>((blue * alpha + bgBlue * (255 - alpha)) / 255);
    }
    return static_cast<uint16_t>(((red & 0xF8) << 8) |
                                 ((green & 0xFC) << 3) | (blue >> 3));
  }
  return background;
}

bool WallpaperService::optimize(const char* sourceLvglPath,
                                uint16_t fillColor) {
  if (!storage_ || !storage_->mounted() || !sourceLvglPath) return false;
  storage_->removePath(TEMP_SD_PATH);

  lv_image_decoder_dsc_t source;
  if (lv_image_decoder_open(&source, sourceLvglPath, nullptr) != LV_RESULT_OK)
    return false;

  lv_fs_file_t output;
  if (lv_fs_open(&output, TEMP_LVGL_PATH, LV_FS_MODE_WR) != LV_FS_RES_OK) {
    lv_image_decoder_close(&source);
    return false;
  }

  Header header = {{'O', 'W', 'P', '1'}, WIDTH, HEIGHT, STRIP_LINES,
                   FORMAT_RGB565,
                   static_cast<uint32_t>(WIDTH) * HEIGHT * 2};
  bool success = writeExact(output, &header, sizeof(header));
  uint16_t row[WIDTH];
  for (uint16_t x = 0; x < WIDTH; ++x) row[x] = fillColor;
  for (uint16_t y = 0; success && y < HEIGHT; ++y)
    success = writeExact(output, row, sizeof(row));

  const int32_t sourceWidth = source.header.w;
  const int32_t sourceHeight = source.header.h;
  const int32_t copyWidth = std::min<int32_t>(sourceWidth, WIDTH);
  const int32_t copyHeight = std::min<int32_t>(sourceHeight, HEIGHT);
  const int32_t sourceX = std::max<int32_t>((sourceWidth - WIDTH) / 2, 0);
  const int32_t sourceY = std::max<int32_t>((sourceHeight - HEIGHT) / 2, 0);
  const int32_t destinationX = std::max<int32_t>((WIDTH - sourceWidth) / 2, 0);
  const int32_t destinationY = std::max<int32_t>((HEIGHT - sourceHeight) / 2, 0);
  lv_area_t wanted = {sourceX, sourceY, sourceX + copyWidth - 1,
                      sourceY + copyHeight - 1};
  lv_area_t decodedArea = {LV_COORD_MIN, LV_COORD_MIN, LV_COORD_MIN,
                           LV_COORD_MIN};

  while (success &&
         lv_image_decoder_get_area(&source, &wanted, &decodedArea) ==
             LV_RESULT_OK) {
    lv_draw_buf_t* decoded = const_cast<lv_draw_buf_t*>(source.decoded);
    if (!decoded || !decoded->data) {
      success = false;
      break;
    }
    const lv_color_format_t format =
        static_cast<lv_color_format_t>(decoded->header.cf);
    const uint8_t pixelSize = lv_color_format_get_size(format);
    if (pixelSize != 2 && pixelSize != 3 && pixelSize != 4) {
      success = false;
      break;
    }
    const int32_t left = std::max<int32_t>(decodedArea.x1, wanted.x1);
    const int32_t right = std::min<int32_t>(decodedArea.x2, wanted.x2);
    const int32_t top = std::max<int32_t>(decodedArea.y1, wanted.y1);
    const int32_t bottom = std::min<int32_t>(decodedArea.y2, wanted.y2);
    if (left <= right && top <= bottom) {
      for (int32_t y = top; success && y <= bottom; ++y) {
        const uint8_t* sourcePixel =
            decoded->data + (y - decodedArea.y1) * decoded->header.stride +
            (left - decodedArea.x1) * pixelSize;
        const int32_t pixels = right - left + 1;
        for (int32_t x = 0; x < pixels; ++x)
          row[x] = pixelToRgb565(sourcePixel + x * pixelSize, format,
                                 fillColor);
        const uint32_t destinationOffset =
            sizeof(Header) +
            ((destinationY + y - sourceY) * WIDTH +
             destinationX + left - sourceX) * 2;
        success =
            lv_fs_seek(&output, destinationOffset, LV_FS_SEEK_SET) ==
                LV_FS_RES_OK &&
            writeExact(output, row, pixels * 2);
      }
    }
    yield();
  }

  lv_fs_close(&output);
  lv_image_decoder_close(&source);
  if (!success || !storage_->renamePath(TEMP_SD_PATH, OPTIMIZED_SD_PATH)) {
    storage_->removePath(TEMP_SD_PATH);
    logger_->warning("wallpaper", "OWP optimization failed");
    return false;
  }
  lv_image_cache_drop(OPTIMIZED_LVGL_PATH);
  invalidateCache();
  logger_->info("wallpaper", "optimized wallpaper created");
  return true;
}

void WallpaperService::clearOptimizedFile() {
  lv_image_cache_drop(OPTIMIZED_LVGL_PATH);
  invalidateCache();
  if (storage_) storage_->removePath(OPTIMIZED_SD_PATH);
}

void WallpaperService::invalidateCache() {
  cacheAge_ = 0;
  for (CacheSlot& slot : cache_) {
    slot.strip = UINT16_MAX;
    slot.age = 0;
  }
}

bool WallpaperService::loadStrip(DecodeSession& session, uint16_t strip,
                                 CacheSlot*& result) {
  ++cacheAge_;
  CacheSlot* oldest = &cache_[0];
  for (CacheSlot& slot : cache_) {
    if (slot.strip == strip) {
      slot.age = cacheAge_;
      result = &slot;
      return true;
    }
    if (slot.strip == UINT16_MAX || slot.age < oldest->age) oldest = &slot;
  }
  const uint32_t offset = sizeof(Header) +
                          static_cast<uint32_t>(strip) * WIDTH *
                              STRIP_LINES * 2;
  if (lv_fs_seek(&session.file, offset, LV_FS_SEEK_SET) != LV_FS_RES_OK)
    return false;
  const uint16_t firstLine = strip * STRIP_LINES;
  const uint16_t lines =
      std::min<uint16_t>(STRIP_LINES, HEIGHT - firstLine);
  const uint32_t bytes = static_cast<uint32_t>(lines) * WIDTH * 2;
  uint32_t read = 0;
  if (lv_fs_read(&session.file, oldest->pixels, bytes, &read) != LV_FS_RES_OK ||
      read != bytes)
    return false;
  oldest->strip = strip;
  oldest->age = cacheAge_;
  result = oldest;
  return true;
}

lv_result_t WallpaperService::decoderInfo(lv_image_decoder_t*,
                                          lv_image_decoder_dsc_t* descriptor,
                                          lv_image_header_t* imageHeader) {
  if (!descriptor || descriptor->src_type != LV_IMAGE_SRC_FILE)
    return LV_RESULT_INVALID;
  const char* path = static_cast<const char*>(descriptor->src);
  const char* extension = strrchr(path, '.');
  if (!extension || strcmp(extension, ".owp")) return LV_RESULT_INVALID;
  Header header;
  uint32_t read = 0;
  if (lv_fs_read(&descriptor->file, &header, sizeof(header), &read) !=
          LV_FS_RES_OK ||
      read != sizeof(header) || !validHeader(header))
    return LV_RESULT_INVALID;
  imageHeader->cf = LV_COLOR_FORMAT_RGB565;
  imageHeader->w = header.width;
  imageHeader->h = header.height;
  imageHeader->stride = header.width * 2;
  return LV_RESULT_OK;
}

lv_result_t WallpaperService::decoderOpen(lv_image_decoder_t*,
                                          lv_image_decoder_dsc_t* descriptor) {
  DecodeSession* session = new DecodeSession{};
  if (!session) return LV_RESULT_INVALID;
  if (lv_fs_open(&session->file, static_cast<const char*>(descriptor->src),
                 LV_FS_MODE_RD) != LV_FS_RES_OK) {
    delete session;
    return LV_RESULT_INVALID;
  }
  uint32_t read = 0;
  if (lv_fs_read(&session->file, &session->header, sizeof(Header), &read) !=
          LV_FS_RES_OK ||
      read != sizeof(Header) || !validHeader(session->header)) {
    lv_fs_close(&session->file);
    delete session;
    return LV_RESULT_INVALID;
  }
  descriptor->user_data = session;
  return LV_RESULT_OK;
}

lv_result_t WallpaperService::decoderGetArea(
    lv_image_decoder_t*, lv_image_decoder_dsc_t* descriptor,
    const lv_area_t* fullArea, lv_area_t* decodedArea) {
  if (!active_ || !descriptor || !fullArea || !decodedArea)
    return LV_RESULT_INVALID;
  DecodeSession* session = static_cast<DecodeSession*>(descriptor->user_data);
  if (decodedArea->y1 == LV_COORD_MIN) {
    *decodedArea = *fullArea;
    decodedArea->y2 = std::min<int32_t>(
        fullArea->y2,
        ((fullArea->y1 / STRIP_LINES) + 1) * STRIP_LINES - 1);
  } else {
    decodedArea->y1 = decodedArea->y2 + 1;
    if (decodedArea->y1 > fullArea->y2) return LV_RESULT_INVALID;
    decodedArea->y2 = std::min<int32_t>(
        fullArea->y2,
        ((decodedArea->y1 / STRIP_LINES) + 1) * STRIP_LINES - 1);
  }
  const uint16_t strip = decodedArea->y1 / STRIP_LINES;
  CacheSlot* slot = nullptr;
  if (!active_->loadStrip(*session, strip, slot)) return LV_RESULT_INVALID;
  const uint16_t localLine = decodedArea->y1 % STRIP_LINES;
  session->decoded.data = reinterpret_cast<uint8_t*>(
      &slot->pixels[localLine * WIDTH + decodedArea->x1]);
  session->decoded.unaligned_data = session->decoded.data;
  session->decoded.header = descriptor->header;
  session->decoded.header.w = lv_area_get_width(decodedArea);
  session->decoded.header.h = lv_area_get_height(decodedArea);
  session->decoded.header.stride = WIDTH * 2;
  session->decoded.data_size =
      session->decoded.header.stride * session->decoded.header.h;
  descriptor->decoded = &session->decoded;
  return LV_RESULT_OK;
}

void WallpaperService::decoderClose(lv_image_decoder_t*,
                                    lv_image_decoder_dsc_t* descriptor) {
  DecodeSession* session = static_cast<DecodeSession*>(descriptor->user_data);
  if (!session) return;
  lv_fs_close(&session->file);
  delete session;
  descriptor->user_data = nullptr;
  descriptor->decoded = nullptr;
}
