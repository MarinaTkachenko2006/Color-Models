#include <string>
#include <iostream>
#include <algorithm>
#include <array>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include "stb_image.h"
#include "stb_image_write.h"

#include "image_processing.h"
#include "images.h"


// Загрузка из файловой системы RGB-изображения
bool loadImageRGB(const std::string& path, ImageRGB& out)
{
    int w = 0, h = 0, srcChannels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &srcChannels, 3);

    if (!pixels)
    {
        std::cerr << "stbi_load failed for '" << path << "': " << stbi_failure_reason() << std::endl;
        return false;
    }

    out.width = w;
    out.height = h;
    out.channels = srcChannels;
    out.data.assign(pixels, pixels + static_cast<size_t>(w) * h * 3); // Копирование массива указателей pixels в массив out.data

    stbi_image_free(pixels); // Освобождение буфера, который stbi_load выделил внутри себя через malloc

    return true;
}

// Отображение RGB-изображения вручную по пикселям (неэффективное)
void drawImageByPixels(SDL_Renderer* renderer, const ImageRGB& img,
    float originX = 0, float originY = 0) // Смещение на экране
{
    if (img.empty()) return;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const uint8_t* p = img.at(x, y); // Указатель на текущий пиксель
            SDL_SetRenderDrawColor(renderer, p[0], p[1], p[2], 255);
            SDL_RenderPoint(renderer, originX + x, originY + y);
        }
    }
}

// Отображение Gray-изображения вручную по пикселям (неэффективное)
void drawImageByPixels(SDL_Renderer* renderer, const ImageGray& img,
    float originX = 0, float originY = 0) // Смещение на экране
{
    if (img.empty()) return;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t v = *img.at(x, y); // Указатель на текущий пиксель
            SDL_SetRenderDrawColor(renderer, v, v, v, 255);
            SDL_RenderPoint(renderer, originX + x, originY + y);
        }
    }
}

// Преобразование RGB-изображения в Gray-изображение
ImageGray RGBtoGray(const ImageRGB& img, bool formula) {
    ImageGray imgGr;
    imgGr.channels = 1;
    imgGr.width = img.width;
    imgGr.height = img.height;
    imgGr.data = std::vector<uint8_t>(img.width * img.height);

    if (formula) // NTSC RGB
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                const uint8_t* p = img.at(x, y);
                imgGr.data[static_cast<size_t>(y) * imgGr.width + x] = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
            }
        }
    else // sRGB
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                const uint8_t* p = img.at(x, y);
                imgGr.data[static_cast<size_t>(y) * imgGr.width + x] = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
            }
        }

    return imgGr;
}

// Разница между 2 Gray-изображениями
ImageGray diffGray(const ImageGray& img1, const ImageGray& img2) {
    ImageGray imgDiff; // Результат
    imgDiff.channels = 1;
    imgDiff.width = img1.width;
    imgDiff.height = img1.height;
    imgDiff.data = std::vector<uint8_t>(img1.width * img1.height);

    int minDiff = 255;
    int maxDiff = 0;

    // Поиск отличий
    for (int y = 0; y < img1.height; ++y) {
        for (int x = 0; x < img1.width; ++x) {
            int diff = std::abs(static_cast<int>(*img1.at(x, y)) - static_cast<int>(*img2.at(x, y)));
            imgDiff.data[static_cast<size_t>(y) * imgDiff.width + x] = static_cast<uint8_t>(diff);
            if (diff < minDiff) minDiff = diff;
            if (diff > maxDiff) maxDiff = diff;
        }
    }

    // Нормализация (чтобы лучше было восприятие)
    if (maxDiff > minDiff) {
        const double scale = 255.0 / (maxDiff - minDiff);
        for (auto& v : imgDiff.data)
            v = static_cast<uint8_t>((v - minDiff) * scale + 0.5);
    }
    else std::fill(imgDiff.data.begin(), imgDiff.data.end(), 0);

    return imgDiff;
}

// Преобразование RGB-изображения в HSV-изображение
void RGBconvertHSV(uint8_t R, uint8_t G, uint8_t B, float& H, float& S, float& V)
{
    float NR(R / 255.0f), NG(G / 255.0f), NB(B / 255.0f);

    float MAX(std::max({ NR, NG, NB })), MIN(std::min({ NR, NG, NB }));
    float diff(MAX - MIN);

    V = MAX;
    S = (MAX == 0) ? 0 : (1 - MIN / MAX);
    if (MAX == MIN) H = 0;
    else if (MAX == NR && NG >= NB) H = 60 * (NG - NB) / diff;
    else if (MAX == NR && NG < NB) H = 60 * (NG - NB) / diff + 360;
    else if (MAX == NG) H = 60 * (NB - NR) / diff + 120;
    else H = 60 * (NR - NG) / diff + 240;
}

// Преобразование HSV-изображения в RGB-изображение
void HSVconvertRGB(float H, float S, float V, uint8_t& R, uint8_t& G, uint8_t& B)
{
    int Hi = static_cast<int>(std::floor(H / 60.0f)) % 6;
    float f = H / 60.0f - std::floor(H / 60.0f);

    float p = V * (1.0f - S);
    float q = V * (1.0f - f * S);
    float t = V * (1.0f - (1.0f - f) * S);

    float NR = 0.0f, NG = 0.0f, NB = 0.0f;
    switch (Hi) {
    case 0:
        NR = V;
        NG = t;
        NB = p;
        break;
    case 1:
        NR = q;
        NG = V;
        NB = p;
        break;
    case 2:
        NR = p;
        NG = V;
        NB = t;
        break;
    case 3:
        NR = p;
        NG = q;
        NB = V;
        break;
    case 4:
        NR = t;
        NG = p;
        NB = V;
        break;
    case 5:
        NR = V;
        NG = p;
        NB = q;
        break;
    }

    R = static_cast<uint8_t>(std::clamp(NR * 255.0f + 0.5f, 0.0f, 255.0f));
    G = static_cast<uint8_t>(std::clamp(NG * 255.0f + 0.5f, 0.0f, 255.0f));
    B = static_cast<uint8_t>(std::clamp(NB * 255.0f + 0.5f, 0.0f, 255.0f));
}

// Преобразование RGB-изображения в HSV-изображение
ImageHSV RGBtoHSV(const ImageRGB& img)
{
    ImageHSV out;
    out.width = img.width;
    out.height = img.height;
    out.data.resize(static_cast<size_t>(img.width) * img.height);

    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            const uint8_t* p = img.at(x, y);
            PixelHSV& px = out.at(x, y);
            RGBconvertHSV(p[0], p[1], p[2], px.h, px.s, px.v);
        }
    return out;
}

// Цветокоррекция в пространстве HSV с возвращением получившегося RGB-изображения
ImageRGB applyHSV(const ImageHSV& hsv, float hueShift, float satScale, float valScale)
{
    ImageRGB result;
    result.width = hsv.width;
    result.height = hsv.height;
    result.channels = 3;
    result.data.resize(static_cast<size_t>(hsv.width) * hsv.height * 3);

    for (size_t i = 0; i < hsv.data.size(); ++i) {
        const PixelHSV& p = hsv.data[i];

        float H = p.h + hueShift;
        while (H < 0)    H += 360;
        while (H >= 360) H -= 360;

        float S(std::clamp(p.s * satScale, 0.0f, 1.0f)), V(std::clamp(p.v * valScale, 0.0f, 1.0f));

        uint8_t R, G, B;
        HSVconvertRGB(H, S, V, R, G, B);

        result.data[i * 3 + 0] = R;
        result.data[i * 3 + 1] = G;
        result.data[i * 3 + 2] = B;
    }

    return result;
}

// Гистограмма яркости Gray-изображения
std::array<int, 256> intensityHistogram(const ImageGray& img)
{
    std::array<int, 256> h{};
    for (uint8_t v : img.data) ++h[v];
    return h;
}

// Отрисовка гистограммы
void drawHistogramByTexture(ImDrawList* dl, ImVec2 origin, ImVec2 size, const std::array<int, 256>& hgt, ImU32 color)
{
    int maxH = 0;
    for (int v : hgt) if (v > maxH) maxH = v;
    if (maxH == 0 || size.x <= 0 || size.y <= 0) return;

    ImVec2 plotMin(origin.x, origin.y);
    ImVec2 plotMax(origin.x + size.x, origin.y + size.y);
    float  plotW = plotMax.x - plotMin.x;
    float  plotH = plotMax.y - plotMin.y;
    if (plotW <= 0 || plotH <= 0) return;

    ImU32 axisColor = IM_COL32(200, 200, 200, 255);
    ImU32 textColor = IM_COL32(220, 220, 220, 255);

    float baseY = origin.y + size.y; // Y-координата основания столбиков
    float scale = size.y / static_cast<float>(maxH);
    float dx = size.x / 256.0f;

    for (int v = 0; v < 256; ++v) {
        float xv = origin.x + (v + 0.5f) * dx; // X-координата линии для яркости v
        float barH = hgt[v] * scale;
        if (barH < 1 && hgt[v] > 0) barH = 1;
        dl->AddLine(ImVec2(xv, baseY), ImVec2(xv, baseY - barH), color);
    }

    // Метки по оси X
    int xTicks[5] = { 0, 64, 128, 192, 255 };
    for (int i (0); i < 5; ++i) {
        int v = xTicks[i];
        float xv = plotMin.x + v * (plotW / 255.0f); // Вычисление координаты X метки, такой что 0 соответствует левому краю, а 255 - правому

        dl->AddLine(ImVec2(xv, baseY), ImVec2(xv, baseY + 4), axisColor);

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", v);
        ImVec2 ts = ImGui::CalcTextSize(buf); // Размер подписи в пикселях

        float textX;
        if (i == 0) textX = xv;
        else if (i == 4) textX = xv - ts.x;
        else textX = xv - ts.x * 0.5f;

        dl->AddText(ImVec2(textX, baseY + 6), textColor, buf);
    }


    // Подписи по Y (0 и maxH)
    char bufY[16];
    ImVec2 tsY;

    std::snprintf(bufY, sizeof(bufY), "%d", 0);
    tsY = ImGui::CalcTextSize(bufY);
    dl->AddText(ImVec2(plotMin.x - tsY.x - 6,
        plotMax.y - tsY.y * 0.5f), textColor, bufY);

    std::snprintf(bufY, sizeof(bufY), "%d", maxH);
    tsY = ImGui::CalcTextSize(bufY);
    dl->AddText(ImVec2(plotMin.x - tsY.x - 6,
        plotMin.y - tsY.y * 0.5f), textColor, bufY);


    // Подпись оси X
    const char* xLabel = "Intensity";
    ImVec2 ts = ImGui::CalcTextSize(xLabel);
    dl->AddText(ImVec2(plotMin.x + (plotW - ts.x) * 0.5f,
        plotMax.y), textColor, xLabel);
}

// Уменьшение изображения (для быстрого выполнения Task 3)
// Уменьшает изображение так, чтобы длинная сторона стала не больше, чем maxSide, с сохранением пропорций
ImageRGB downscale(const ImageRGB& src, int maxSide)
{
    if (src.width <= maxSide && src.height <= maxSide) return src;

    float scale = std::min((float)maxSide / src.width, (float)maxSide / src.height);
    int nw = std::max(1, (int)(src.width * scale));
    int nh = std::max(1, (int)(src.height * scale));

    ImageRGB dst;
    dst.width = nw;
    dst.height = nh;
    dst.channels = 3;
    dst.data.resize(nw * nh * 3);

    for (int y = 0; y < nh; ++y) {
        int sy = (int)((float)y * src.height / nh);

        for (int x = 0; x < nw; ++x) {
            int sx = (int)((float)x * src.width / nw);

            const uint8_t* p = src.at(sx, sy);
            uint8_t* q = &dst.data[(y * nw + x) * 3];
            q[0] = p[0];
            q[1] = p[1];
            q[2] = p[2];
        }
    }
    return dst;
}

// Преобразование RGB-изображения в SDL_Texture
SDL_Texture* makeTextureRGB(SDL_Renderer* r, const ImageRGB& img)
{
    if (img.empty()) return nullptr;
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, img.data.data(), img.width * 3);
    return t;
}

// Преобразование Gray-изображения в SDL_Texture
SDL_Texture* makeTextureGray(SDL_Renderer* r, const ImageGray& img)
{
    if (img.empty()) return nullptr;
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
    if (!t) return nullptr;

    std::vector<uint8_t> rgb(static_cast<size_t>(img.width) * img.height * 3);
    for (size_t i = 0; i < img.data.size(); ++i) {
        uint8_t v = img.data[i];
        rgb[i * 3 + 0] = v;
        rgb[i * 3 + 1] = v;
        rgb[i * 3 + 2] = v;
    }
    SDL_UpdateTexture(t, nullptr, rgb.data(), img.width * 3);
    return t;
}

// Сохранение в PNG
bool saveImagePNG(const std::string& path, const ImageRGB& img)
{
    if (img.empty()) return false;
    return stbi_write_png(path.c_str(), img.width, img.height, 3,
        img.data.data(), img.width * 3) != 0;
}

void drawImageRGBByPixels(SDL_Renderer* renderer, const ImageRGB& img,
    float originX, float originY,
    float areaW, float areaH)
{
    if (img.empty()) return;
    if (areaW < 1.f || areaH < 1.f) return;

    // Обрезаем всё, что вылезает за границы области
    SDL_Rect clip{(int)originX, (int)originY, (int)areaW, (int)areaH };
    SDL_SetRenderClipRect(renderer, &clip);

    float s = std::min(areaW / (float)img.width,
        areaH / (float)img.height);
    float dw = img.width * s;
    float dh = img.height * s;
    float dx = originX + (areaW - dw) * 0.5f;
    float dy = originY + (areaH - dh) * 0.5f;

    for (int py = 0; py < (int)dh; ++py) {
        int sy = (int)(py / s);
        if (sy >= img.height) sy = img.height - 1;

        for (int px = 0; px < (int)dw; ++px) {
            int sx = (int)(px / s);
            if (sx >= img.width) sx = img.width - 1;

            const uint8_t* p = img.at(sx, sy);
            SDL_SetRenderDrawColor(renderer, p[0], p[1], p[2], 255);
            SDL_RenderPoint(renderer, dx + px, dy + py);
        }
    }

    SDL_SetRenderClipRect(renderer, nullptr);
}