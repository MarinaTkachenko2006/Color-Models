// Сторонние библиотеки
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

// Стандартные библиотеки
#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>
#include <array>
#include <algorithm>
#include <iostream>

// Наши библиотеки
#include "images.h"
#include "image_processing.h"
#include "tasks.h"
#include "dialog_windows.h"

static bool showWindow2 (false);   // Флаг открытости окна заданий
static bool manualDrawing (false);  // Флаг ручной или текстурной отрисовки

enum class ActiveTask { None, Task1, Task2, Task3 }; // Перечисление возможных активных заданий

SDL_Window* window = nullptr; // Указатель на окно
SDL_Renderer* renderer = nullptr; // Указатель на рендерер для отрисовки
SDL_Event eventer; // Контролёр событий

struct MainImageCallback // Callback, вызываемый ImGui внутри RenderDrawData
{
    const ImageRGB* image = nullptr;
    SDL_Renderer* renderer = nullptr;
    ImVec2 origin;
    ImVec2 area;
};

static void SDLCALL drawMainImageCallback(const ImDrawList*, const ImDrawCmd* cmd)
{
    auto* d = static_cast<MainImageCallback*>(cmd->UserCallbackData);
    if (!d || !d->image || d->image->empty()) return;

    drawImageRGBByPixels(d->renderer, *d->image,
        d->origin.x, d->origin.y,
        d->area.x, d->area.y);
}

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) { // вызов SDL-подсистемы
        std::cerr << "SDL_Init failed" << std::endl;
        return -1;
    }
    MainImageCallback mainCb;

    window = SDL_CreateWindow("ImGui Window", 1000, 700, SDL_WINDOW_RESIZABLE); // Создание окна
    if (!window) { std::cerr << "SDL_CreateWindow failed" << std::endl; return 1; }

    renderer = SDL_CreateRenderer(window, nullptr); // Создание рендерера
    if (!renderer) { std::cerr << "SDL_CreateRenderer failed" << std::endl; return 1; }

    IMGUI_CHECKVERSION(); // Gроверки версий файлов .h библиотеки ImGUI
    ImGui::CreateContext(); // Создание глобального контекста для отрисовки
    ImGui::StyleColorsDark(); // Установка тёмного стиля

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer); // Dызов слоя, отвечающего за отрисовку в ImGUI

    ImGuiIO& io = ImGui::GetIO(); // Получение глобального объекта ввода-вывода (для контроля текущего состояния программы)

    ImageRGB image; // Изображение, загруженное в RAM
    SDL_Texture* texImage = nullptr; // Текстура загруженного изображения

    TaskInterface* currentTask = nullptr; // Текущая активная задача
    ActiveTask activeTask = ActiveTask::None; // Тип текущей активной задачи

    FileDialogState dlg; // Диалоговое окно для выбора загрузки файла
    const SDL_DialogFileFilter filters[2] = {
        { "Images", "png;jpg;jpeg;bmp;tga;gif;psd;hdr;pic;pnm" },
        { "All files", "*" } };

    bool running (true); // Флаг работы программы

    // Область ручной отрисовки главного изображения
    ImVec2 mainOrigin{ 0, 0 }; // Текущая позиция курсора ImGui
    ImVec2 mainArea{ 0, 0 }; // Текущее занятое пространство
    bool mainManualDrawRequested (false);

    // Область ручной отрисовки задачи
    ImVec2 manualOrigin{ 0, 0 }; // Текущая позиция курсора ImGui
    ImVec2 manualArea{ 0, 0 }; // Текущее занятое пространство
    bool taskManualDrawRequested (false);

    while (running) { // Пока не закрыли программу
        while (SDL_PollEvent(&eventer)) { // Контроль событий
            ImGui_ImplSDL3_ProcessEvent(&eventer);
            if (eventer.type == SDL_EVENT_QUIT) running = false;
        }

        // Загрузка изображения
        if (dlg.ready) { // Диалоговое окно закрылось
            if (dlg.ok && loadImageRGB(dlg.path, image)) { // Пользователь выбрал файл, и он был успешно загружен
                if (texImage) SDL_DestroyTexture(texImage);
                texImage = makeTextureRGB(renderer, image);

                delete currentTask;  currentTask = nullptr;
                showWindow2 = false;
                activeTask = ActiveTask::None;
            }
            dlg.reset(); // Сброс диалогового окна
        }

        // Обработка сохранения (только для задания 3)
        if (auto* t3 = dynamic_cast<Task3*>(currentTask)) {
            if (t3->saveDlg.ready) { // Диалоговое окно закрылось
                if (t3->saveDlg.ok && !t3->applyed.empty()) { // Пользователь выбрал файл, и он был успешно сохранён
                    if (!saveImagePNG(t3->saveDlg.path, t3->applyed))
                        std::cerr << "Save PNG failed: " << t3->saveDlg.path << std::endl;
                }
                t3->saveDlg.reset(); // Сброс диалогового окна
            }
        }

        mainManualDrawRequested = taskManualDrawRequested = false;

        ImGui_ImplSDLRenderer3_NewFrame(); // Подготовка внутренних буферов рендера ImGui
        ImGui_ImplSDL3_NewFrame(); // Передача накопленных событий ввода и актуализация состояния мыши/клавиатуры
        ImGui::NewFrame(); // Создание нового кадра, в течение которого формируется список виджетов

        // Отрисовка главного окна
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always); // 10 пикселей от левого верхнего угла окна
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20, io.DisplaySize.y - 20), ImGuiCond_Always);
        ImGui::Begin("Color Models", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

        const float buttonPanelW = 260; // Ширина правой панели с кнопками
        ImVec2 contentAvail = ImGui::GetContentRegionAvail(); // Сколько места сейчас доступно внутри главного окна — ширина и высота в пикселях

        // Левая область — изображение
        ImGui::BeginChild("##image_area", ImVec2(contentAvail.x - buttonPanelW - 10, 0), true);

        if (!image.empty() && texImage) { // Есть ли что рисовать
            // Расчёт масштаба и размеров
            ImVec2 avail = ImGui::GetContentRegionAvail(); // Сколько свободного места внутри текущей child-области
            float scale = std::min(avail.x / image.width, avail.y / image.height); // Коэффициент вписывания с сохранением пропорций
            ImVec2 sz(image.width * scale, image.height * scale); // Реальный размер, в который картинка будет выведена с сохранением пропорций

            // Центрирование
            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(
                pos.x + (avail.x - sz.x) * 0.5,
                pos.y + (avail.y - sz.y) * 0.5));

            if (manualDrawing) {
                mainOrigin = ImGui::GetCursorScreenPos();
                mainArea = sz;

                ImGui::Dummy(sz);

                // Заполняем данные и просим ImGui вызвать наш callback в нужном слое
                mainCb.image = &image;
                mainCb.renderer = renderer;
                mainCb.origin = mainOrigin;
                mainCb.area = mainArea;

                ImGui::GetWindowDrawList()->AddCallback(
                    &drawMainImageCallback, &mainCb);
            }
            else ImGui::Image((ImTextureID)(intptr_t)texImage, sz);
        }
        else ImGui::TextUnformatted("Load an image to see it here");

        ImGui::EndChild();

        ImGui::SameLine();

        // Правая область — кнопки
        ImGui::BeginChild("##button_panel", ImVec2(buttonPanelW, 0), true);

        if (ImGui::Button("load image")) {
            if (!dlg.pending) {
                dlg.pending = true;
                SDL_ShowOpenFileDialog(onFileDialogResult, &dlg, window,
                    filters, static_cast<int>(sizeof(filters) / sizeof(filters[0])),
                    nullptr, false);
            }
        }

        // Формирование контекста задачи
        AppContext ctx;
        ctx.renderer = renderer;
        ctx.window = window;
        ctx.image = &image;
        ctx.texImage = texImage;

        if (ImGui::Button("Task 1")) { // Кнопка Task 1 нажата
            delete currentTask;
            currentTask = image.empty() ? nullptr : new Task1();
            activeTask = ActiveTask::Task1;
            showWindow2 = true;
            if (currentTask) currentTask->prepare(ctx);
        }
        if (ImGui::Button("Task 2")) { // Кнопка Task 2 нажата
            delete currentTask;
            currentTask = image.empty() ? nullptr : new Task2();
            activeTask = ActiveTask::Task2;
            showWindow2 = true;
            if (currentTask) currentTask->prepare(ctx);
        }
        if (ImGui::Button("Task 3")) { // Кнопка Task 3 нажата
            delete currentTask;
            currentTask = image.empty() ? nullptr : new Task3();
            activeTask = ActiveTask::Task3;
            showWindow2 = true;
            if (currentTask) currentTask->prepare(ctx);
        }

        ImGui::EndChild();
        ImGui::End();

        // Установка окна задачи
        if (showWindow2) {
            const char* title =
                activeTask == ActiveTask::Task1 ? "Task 1" :
                activeTask == ActiveTask::Task2 ? "Task 2" :
                activeTask == ActiveTask::Task3 ? "Task 3" : "None";

            ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(700, 800), ImGuiCond_FirstUseEver);
            ImGui::Begin(title, &showWindow2, ImGuiWindowFlags_HorizontalScrollbar);

            if (!currentTask) {
                ImGui::TextUnformatted("Load an image first");
            }
            else if (manualDrawing) {
                // Контроллеры
                currentTask->drawControls(ctx);

                // Пересчёт превью
                if (auto* t3 = dynamic_cast<Task3*>(currentTask))
                    t3->updatePreview(renderer);

                // Захват оставшегося места под SDL-отрисовку
                manualOrigin = ImGui::GetCursorScreenPos();
                manualArea = ImGui::GetContentRegionAvail();
                taskManualDrawRequested = true;

                ImGui::Dummy(manualArea);
            }
            else {
                // Текстурный режим
                currentTask->drawControls(ctx);
                if (auto* t3 = dynamic_cast<Task3*>(currentTask))
                    t3->updatePreview(renderer);
                currentTask->drawByTexture(ctx);
            }

            ImGui::End();
        }

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        // Отрисовка окна задачи
        if (taskManualDrawRequested && currentTask) {
            ctx.originX = manualOrigin.x;
            ctx.originY = manualOrigin.y;
            ctx.areaW = manualArea.x;
            ctx.areaH = manualArea.y;

            currentTask->drawByPixels(ctx);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    delete currentTask;

    if (texImage) SDL_DestroyTexture(texImage);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}