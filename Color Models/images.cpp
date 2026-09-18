#include "images.h"

bool Image::empty() const noexcept { return width == 0 || height == 0; }
size_t Image::pixelNumber() const noexcept { return width * height; }

uint8_t* ImageRGB::at(int x, int y) { return &data[(static_cast<size_t>(y) * width + x) * 3]; }
const uint8_t* ImageRGB::at(int x, int y) const { return &data[(static_cast<size_t>(y) * width + x) * 3]; }

uint8_t* ImageGray::at(int x, int y) { return &data[(static_cast<size_t>(y) * width + x)]; }
const uint8_t* ImageGray::at(int x, int y) const { return &data[(static_cast<size_t>(y) * width + x)]; }

PixelHSV& ImageHSV::at(int x, int y) { return data[static_cast<size_t>(y) * width + x]; }
const PixelHSV& ImageHSV::at(int x, int y) const { return data[static_cast<size_t>(y) * width + x]; }
