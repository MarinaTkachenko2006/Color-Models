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





// Класс единого контекста, который получает любая задача
class AppContext {
public:
    SDL_Renderer* renderer = nullptr;
    SDL_Window* window = nullptr;
    const ImageRGB* image = nullptr; // Исходное изображение
    SDL_Texture* texImage = nullptr; // Текстура исходного изображения

    // Прямоугольник области для ручной отрисовки
    float originX = 0, originY = 0, areaW = 0.f, areaH = 0;
};

// Интерфейс класса задачи
class TaskInterface {
public:
    virtual ~TaskInterface() = default;

    // Тяжёлые вычисления 1 раз при 1 открытии задачи
    virtual void prepare(const AppContext& ctx) = 0;

    // Отрисовка через ImGui (рисует сам ImGui)
    virtual void drawByTexture(const AppContext& ctx) = 0;

    // Ручная по-пиксельная отрисовка через SDL
    virtual void drawByPixels(const AppContext& ctx) = 0;

    virtual void drawControls(const AppContext& ctx) {}
};

// Класс задачи 1
class Task1 : public TaskInterface {
public:
    // Полутоновые изображения, полученные разными формула
    ImageGray img1, img2;
    // Абсолютная разница между texImg1 и texImg2
    ImageGray imgDiff;
    std::array<int, 256> h1{}, h2{}, hd{}; // Гистограммы для img1 (NTSC), img2 (sRGB) и imgDiff

    // Текстуры полутоновых изображений
    SDL_Texture* texImg1 = nullptr;
    SDL_Texture* texImg2 = nullptr;
    SDL_Texture* texDiff = nullptr;

    ~Task1() noexcept override;
    void freeTextures() noexcept;

    void prepare(const AppContext& ctx) override;
    void drawByTexture(const AppContext& ctx) override;
    void drawByPixels(const AppContext& ctx) override;
};

class Task2 : public TaskInterface {
public:
    // RGB изображения для каналов R, G, B (каждый канал в своём цвете)
    ImageRGB imgR, imgG, imgB;

    // Текстуры для каждого канала
    SDL_Texture* texR = nullptr;
    SDL_Texture* texG = nullptr;
    SDL_Texture* texB = nullptr;

    // Гистограммы для каждого канала
    std::array<int, 256> histR{}, histG{}, histB{};

    ~Task2() noexcept override;
    void freeTextures() noexcept;

    void prepare(const AppContext& ctx) override;
    void drawByTexture(const AppContext& ctx) override;
    void drawByPixels(const AppContext& ctx) override;
};

class Task3 : public TaskInterface {
public:
    ImageHSV hsv;
    ImageHSV hsvPreview; // Уменьшенное HSV-изображение (для ускорения работы)
    ImageRGB applyed; // Полный RGB-результат
    ImageRGB applyedPreview; // Уменьшенный RGB-результат
    SDL_Texture* texApplyed = nullptr; // Текстура для applyedPreview

    float hueShift = 0, satScale = 1, valScale = 1;  // Сдвиг оттенка, градусы [-180; 180], множитель насыщенности [0; 2], множитель яркости [0; 2]

    // Предыдущие значения слайдеров (чтобы не пересчитывать превью зря)
    float lastH = 1e9f, lastS = 1e9f, lastV = 1e9f;

    SaveDialogState saveDlg; // Состояние диалогового окна сохранения

    // Фильтры файлов для диалогового окна сохранения
    static const SDL_DialogFileFilter saveFilters[2];

    ~Task3() noexcept override;
    void freeTextures() noexcept;
    void updatePreview(SDL_Renderer* renderer);
    void prepare(const AppContext& ctx) override;
    void drawByTexture(const AppContext& ctx) override;
    void drawByPixels(const AppContext& ctx) override;
    void drawControls(const AppContext& ctx) override;
};


#endif // !__TASKS_IMD_MAR_VIK__
