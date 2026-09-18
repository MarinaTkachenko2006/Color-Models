#ifndef __IMAGES_IMD_MAR_VIK__
#define __IMAGES_IMD_MAR_VIK__

#include <vector>

struct PixelHSV { float h = 0, s = 0, v = 0; };  // h = [0;360); s,v =[0;1)

class ImageRGB {
public:
    int width = 0; // Ширина изображения
    int height = 0; // Высота изображения
    int channels = 0; // Количество каналов
    std::vector<uint8_t> data; // Вектор битов размера width * height * 3

    ImageRGB() noexcept = default;

    bool empty() const noexcept;
    uint8_t* at(int x, int y);
    const uint8_t* at(int x, int y) const;
};

class ImageGray {
public:
    int width = 0; // Ширина изображения
    int height = 0; // Высота изображения
    int channels = 0; // Количество каналов
    std::vector<uint8_t> data; // Вектор битов размера width * height

    ImageGray() noexcept = default;

    bool empty() const noexcept;
    uint8_t* at(int x, int y);
    const uint8_t* at(int x, int y) const;
};

class ImageHSV
{
public:
    int width = 0; // Ширина изображения
    int height = 0; // Высота изображения
    std::vector<PixelHSV> data; // Вектор HSV-троек

    ImageHSV() noexcept = default;

    bool empty() const noexcept;
    PixelHSV& at(int x, int y);
    const PixelHSV& at(int x, int y) const;
};


#endif // !__IMAGES_IMD_MAR_VIK__
