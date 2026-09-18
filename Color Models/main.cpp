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

static bool showWindow2 = false; // Флаг открытости окна заданий
enum class ActiveTask { None, Task1, Task2, Task3 };
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Event eventer;

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed" << std::endl;
        return -1;
    }

    window = SDL_CreateWindow("ImGui Window", 1000, 700, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed" << std::endl;
        return 1;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
    {
        std::cerr << "SDL_CreateRenderer failed" << std::endl;
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1;
    ImGui::GetStyle().Colors[ImGuiCol_ChildBg].w = 1;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    ImGuiIO& io = ImGui::GetIO(); // Глобальный ввод-вывод

    ImageRGB image;
    SDL_Texture* texImage = nullptr;
    
    Task1* task1 = nullptr;
    Task2* task2 = nullptr;
    Task3* task3 = nullptr;

    FileDialogState dlg; // Диалоговое окно выбора файла
    ActiveTask activeTask = ActiveTask::None;
    const SDL_DialogFileFilter filters[] = { { "Images", "png;jpg;jpeg;bmp;tga;gif;psd;hdr;pic;pnm" }, { "All files", "*" } }; // Флаги диалогового окна
    bool running(true); // Флаг работы программы

    while (running) {
        while (SDL_PollEvent(&eventer)) {
            ImGui_ImplSDL3_ProcessEvent(&eventer);
            if (eventer.type == SDL_EVENT_QUIT) // Обработка события того, что приложение закрывается
                running = false;
        }

        if (dlg.ready) { // Диалог закрылся
            if (dlg.ok && loadImageRGB(dlg.path, image)) { // Файл выбран и успешно загружен

                // Освобождение старых текстур
                if (texImage) SDL_DestroyTexture(texImage);
                texImage = makeTextureRGB(renderer, image);

                delete task1; task1 = nullptr;
                delete task2; task2 = nullptr;
                delete task3; task3 = nullptr;

                showWindow2 = false;
                activeTask = ActiveTask::None;
            }
            dlg.reset();
        }

        if (task3 && task3->saveDlg.ready) {
            if (task3->saveDlg.ok && !task3->applyed.empty()) {
                if (!saveImagePNG(task3->saveDlg.path, task3->applyed))
                    std::cerr << "Save PNG failed: " << task3->saveDlg.path << std::endl;
            }
            task3->saveDlg.reset();
        }


        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Панель и кнопки
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20, io.DisplaySize.y - 20), ImGuiCond_Always);

        ImGui::Begin("Color Models", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

        const float buttonPanelW = 260;
        ImVec2 contentAvail = ImGui::GetContentRegionAvail();

        // Левая область - отображение загруженного изображения
        ImGui::BeginChild("##image_area", ImVec2(contentAvail.x - buttonPanelW - 10, 0), true);

        if (!image.empty() && texImage) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float scale = std::min(avail.x / image.width, avail.y / image.height);
            ImVec2 sz(image.width * scale, image.height * scale);

            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(pos.x + (avail.x - sz.x) * 0.5f,  pos.y + (avail.y - sz.y) * 0.5f));
            ImGui::Image((ImTextureID)(intptr_t)texImage, sz);
        }
        else {
            ImGui::TextUnformatted("Load an image to see it here");
        }

        ImGui::EndChild();

        // Правая область - кнопки
        ImGui::SameLine();
        ImGui::BeginChild("##button_panel", ImVec2(buttonPanelW, 0), true);

        if (ImGui::Button("load image")) {
            if (!dlg.pending) {
                dlg.pending = true;
                SDL_ShowOpenFileDialog(
                    onFileDialogResult, &dlg, window,
                    filters,
                    static_cast<int>(sizeof(filters) / sizeof(filters[0])),
                    nullptr, false);
            }
        }

        if (ImGui::Button("Task 1")) {
            delete task1; task1 = nullptr;
            delete task2; task2 = nullptr;
            delete task3; task3 = nullptr;

            if (!image.empty()) {
                task1 = new Task1();
                task1->prepare(renderer, image);
            }

            activeTask = ActiveTask::Task1;
            showWindow2 = true;
        }
        if (ImGui::Button("Task 2")) {
            delete task1; task1 = nullptr;
            delete task2; task2 = nullptr;
            delete task3; task3 = nullptr;

            if (!image.empty()) {
                task2 = new Task2();
                task2->prepare(renderer, image);
            }


            activeTask = ActiveTask::Task2;
            showWindow2 = true;
        }
        if (ImGui::Button("Task 3")) {
            delete task1; task1 = nullptr;
            delete task2; task2 = nullptr;
            delete task3; task3 = nullptr;

            if (!image.empty()) {
                task3 = new Task3();
                task3->prepare(renderer, image);
            }


            activeTask = ActiveTask::Task3;
            showWindow2 = true;
        }


        ImGui::EndChild();
        ImGui::End();

        if (showWindow2) {
            const char* title =
                activeTask == ActiveTask::Task1 ? "Task 1" :
                activeTask == ActiveTask::Task2 ? "Task 2" :
                activeTask == ActiveTask::Task3 ? "Task 3" : "None";

            ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(700, 800), ImGuiCond_FirstUseEver);
            ImGui::Begin(title, &showWindow2, ImGuiWindowFlags_HorizontalScrollbar);

            switch (activeTask) {
            case ActiveTask::Task1:
                if (task1) task1->draw(image);
                else       ImGui::TextUnformatted("Load an image first");
                break;

            case ActiveTask::Task2:
                if (task2) task2->draw(image);
                else       ImGui::TextUnformatted("Load an image first");
                break;

            case ActiveTask::Task3:
                if (task3) task3->draw(renderer, window, image, texImage);
                else       ImGui::TextUnformatted("Load an image first");
                break;

            case ActiveTask::None:
            default:
                ImGui::TextUnformatted("No task selected");
                break;
            }

            ImGui::End();
        }

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    delete task1;
    delete task2;
    delete task3;

    if (texImage) SDL_DestroyTexture(texImage);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}