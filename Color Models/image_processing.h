#ifndef  __IMAGE_PROCESSING_IMD_MAR_VIK__
#define __IMAGE_PROCESSING_IMD_MAR_VIK__

#include <string>
#include <iostream>
#include <algorithm>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include "image_processing.h"
#include "images.h"
#include "stb_image.h"
#include "stb_image_write.h"

// Загрузка из файловой системы RGB-изображения
bool loadImageRGB(const std::string& path, ImageRGB& out);

// Преобразование RGB-изображения в Gray-изображение
ImageGray RGBtoGray(const ImageRGB& img, bool formula);
// Разница между 2 GRAY-изображениями
ImageGray diffGray(const ImageGray& img1, const ImageGray& img2);

// Преобразование RGB-изображения в HSV-изображение
void RGBconvertHSV(uint8_t R, uint8_t G, uint8_t B, float& H, float& S, float& V);
// Преобразование HSV-изображения в RGB-изображение
void HSVconvertRGB(float H, float S, float V, uint8_t& R, uint8_t& G, uint8_t& B);

// Преобразование RGB-изображения в HSV-изображение
ImageHSV RGBtoHSV(const ImageRGB& img);

// Цветокоррекция в пространстве HSV с возвращением получившегося RGB-изображения
ImageRGB applyHSV(const ImageHSV& hsv, float hueShift, float satScale, float valScale);

// Гистограмма яркости Gray-изображения
std::array<int, 256> intensityHistogram(const ImageGray& img);

// Отрисовка гистограммы при помощи текстуры
void drawHistogramByTexture(ImDrawList* dl, ImVec2 origin, ImVec2 size, const std::array<int, 256>& hgt, ImU32 color);

// Уменьшение изображения (для быстрого выполнения Task 3)
ImageRGB downscale(const ImageRGB& src, int maxSide);

// Преобразование RGB-изображения в SDL_Texture
SDL_Texture* makeTextureRGB(SDL_Renderer* r, const ImageRGB& img);
// Преобразование Gray-изображения в SDL_Texture
SDL_Texture* makeTextureGray(SDL_Renderer* r, const ImageGray& img);

// Сохранение в PNG
bool saveImagePNG(const std::string& path, const ImageRGB& img);

void drawImageRGBByPixels(SDL_Renderer* renderer, const ImageRGB& img, float originX, float originY, float areaW, float areaH);


// Выделение каналов R, G, B из RGB-изображения (каждый в своём цвете)
ImageRGB extractChannelR(const ImageRGB& img);
ImageRGB extractChannelG(const ImageRGB& img);
ImageRGB extractChannelB(const ImageRGB& img);

// Гистограммы каналов RGB
std::array<int, 256> histogramChannelR(const ImageRGB& img);
std::array<int, 256> histogramChannelG(const ImageRGB& img);
std::array<int, 256> histogramChannelB(const ImageRGB& img);

#endif // ! __IMAGE_PROCESSING_IMD_MAR_VIK__
