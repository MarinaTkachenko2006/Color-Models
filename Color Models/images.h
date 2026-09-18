#ifndef __IMAGES_IMD_MAR_VIK__
#define __IMAGES_IMD_MAR_VIK__

#include <vector>

struct PixelHSV { float h = 0, s = 0, v = 0; };  // h = [0;360); s,v =[0;1)

// Базовый класс изображения
class Image {
public:
    int width = 0, height = 0, channels = 0;
    Image() = default;

    bool empty() const noexcept;
    size_t pixelNumber() const noexcept;
};

// Класс изображения в RGB-моделе
class ImageRGB : public Image {
public:
    std::vector<uint8_t> data; // Вектор байтов размера width * height * 3

    ImageRGB() noexcept = default;

    uint8_t* at(int x, int y);
    const uint8_t* at(int x, int y) const;
};

// Класс полутонового изображения
class ImageGray : public Image {
public:
    std::vector<uint8_t> data; // Вектор байт размера width * height

    ImageGray() noexcept = default;

    uint8_t* at(int x, int y);
    const uint8_t* at(int x, int y) const;
};

// Класс изображения в модели HSV
class ImageHSV : public Image {
public:
    std::vector<PixelHSV> data; // Вектор HSV-троек

    ImageHSV() noexcept = default;

    PixelHSV& at(int x, int y);
    const PixelHSV& at(int x, int y) const;
};


#endif // !__IMAGES_IMD_MAR_VIK__
