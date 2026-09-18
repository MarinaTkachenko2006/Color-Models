#ifndef __TASKS_IMD_MAR_VIK__
#define __TASKS_IMD_MAR_VIK__

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <array>

#include "images.h"
#include "image_processing.h"
#include "dialog_windows.h"

class Task1 {
public:
    ImageGray img1, img2, imgDiff;
    std::array<int, 256> h1{}, h2{}, hd{}; // Гистограммы для img1 (NTSC), img2 (sRGB) и imgDiff

    // Текстуры полутоновых изображений, полученные разными формулами
    SDL_Texture* texImg1 = nullptr;
    SDL_Texture* texImg2 = nullptr;
    SDL_Texture* texDiff = nullptr; // Абсолютная разница между texImg1 и texImg2

    ~Task1() noexcept;

    // Уничтожает все три текстуры и обнуляет указатели
    void freeTextures() noexcept;
    // Перевод в оттенки серого двумя формулами
    // Вычисление разности
    // Вычисление трёх гистограмм
    // Создание трёх текстур
    void prepare(SDL_Renderer* renderer, const ImageRGB& image);

    // Отрисовка Task 1 
    void draw(const ImageRGB& image) const;
};

class Task2 {
public:
    ~Task2() noexcept = default;

    void prepare(SDL_Renderer* /*renderer*/, const ImageRGB& /*image*/);

    void draw(const ImageRGB& /*image*/) const;
};

class Task3 {
public:
    ImageHSV hsv;
    ImageHSV hsvPreview; // Уменьшенное HSV-изображение (для ускорения работы)
    ImageRGB applyed; // Полный RGB-результат
    ImageRGB applyedPreview; // Уменьшенный RGB-результат
    SDL_Texture* texApplyed = nullptr; // Текстура для applyedPreview

    float hueShift = 0.f;  // Сдвиг оттенка, градусы [-180; 180]
    float satScale = 1.f;  // Множитель насыщенности [0; 2]
    float valScale = 1.f;  // Множитель яркости [0; 2]

    // Предыдущие значения слайдеров (чтобы не пересчитывать превью зря)
    float lastH = 1e9f; // Предыдущий hueShift
    float lastS = 1e9f; // Предыдущий satScale
    float lastV = 1e9f; // Предыдущий valScale

    SaveDialogState saveDlg; // Состояние диалогового окна сохранения

    // Фильтры файлов для диалогового окна сохранения
    static const SDL_DialogFileFilter saveFilters[2];

    ~Task3() noexcept;

    void freeTextures() noexcept;

    void prepare(SDL_Renderer* /*renderer*/, const ImageRGB& image);

    void draw(SDL_Renderer* renderer, SDL_Window* window, const ImageRGB& image, SDL_Texture* texOriginal);
};


#endif // !__TASKS_IMD_MAR_VIK__
