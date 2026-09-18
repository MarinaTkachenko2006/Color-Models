#include "images.h"

bool ImageRGB::empty() const noexcept { return data.empty(); }
uint8_t* ImageRGB::at(int x, int y) { return &data[(static_cast<size_t>(y) * width + x) * 3]; }
const uint8_t* ImageRGB::at(int x, int y) const { return &data[(static_cast<size_t>(y) * width + x) * 3]; }

bool ImageGray::empty() const noexcept { return data.empty(); }
uint8_t* ImageGray::at(int x, int y) { return &data[(static_cast<size_t>(y) * width + x)]; }
const uint8_t* ImageGray::at(int x, int y) const { return &data[(static_cast<size_t>(y) * width + x)]; }

bool ImageHSV::empty() const noexcept { return data.empty(); }
PixelHSV& ImageHSV::at(int x, int y) { return data[static_cast<size_t>(y) * width + x]; }
const PixelHSV& ImageHSV::at(int x, int y) const { return data[static_cast<size_t>(y) * width + x]; }
