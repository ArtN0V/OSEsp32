#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../kernel/Logger.h"
#include "StorageService.h"

class WallpaperService {
 public:
  static constexpr uint16_t WIDTH = 320;
  static constexpr uint16_t HEIGHT = 204;
  static constexpr uint8_t STRIP_LINES = 20;
  static constexpr uint8_t CACHE_SLOTS = 2;
  static constexpr const char* OPTIMIZED_SD_PATH =
      "/OSEsp32/Wallpapers/desktop.owp";
  static constexpr const char* OPTIMIZED_LVGL_PATH =
      "S:/OSEsp32/Wallpapers/desktop.owp";

  void begin(StorageService& storage, Logger& logger);
  bool optimize(const char* sourceLvglPath, uint16_t fillColor);
  void clearOptimizedFile();
  void invalidateCache();

 private:
  struct __attribute__((packed)) Header {
    char magic[4];
    uint16_t width;
    uint16_t height;
    uint16_t stripLines;
    uint16_t colorFormat;
    uint32_t dataSize;
  };

  struct CacheSlot {
    alignas(4) uint16_t pixels[WIDTH * STRIP_LINES];
    uint16_t strip = UINT16_MAX;
    uint32_t age = 0;
  };

  struct DecodeSession {
    lv_fs_file_t file;
    Header header;
    lv_draw_buf_t decoded;
  };

  static WallpaperService* active_;
  StorageService* storage_ = nullptr;
  Logger* logger_ = nullptr;
  lv_image_decoder_t* decoder_ = nullptr;
  CacheSlot cache_[CACHE_SLOTS];
  uint32_t cacheAge_ = 0;

  static bool validHeader(const Header& header);
  static uint16_t pixelToRgb565(const uint8_t* pixel,
                                lv_color_format_t format,
                                uint16_t background);
  bool loadStrip(DecodeSession& session, uint16_t strip, CacheSlot*& slot);
  static lv_result_t decoderInfo(lv_image_decoder_t* decoder,
                                 lv_image_decoder_dsc_t* descriptor,
                                 lv_image_header_t* header);
  static lv_result_t decoderOpen(lv_image_decoder_t* decoder,
                                 lv_image_decoder_dsc_t* descriptor);
  static lv_result_t decoderGetArea(lv_image_decoder_t* decoder,
                                    lv_image_decoder_dsc_t* descriptor,
                                    const lv_area_t* fullArea,
                                    lv_area_t* decodedArea);
  static void decoderClose(lv_image_decoder_t* decoder,
                           lv_image_decoder_dsc_t* descriptor);
};
