#ifndef  __DIALOG_WINDOWS_IMD_MAR_VIK__
#define __DIALOG_WINDOWS_IMD_MAR_VIK__



#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <string>

// Состояние диалога выбора файла
struct FileDialogState
{
    bool pending = false; // true, пока диалог открыт и ответа ещё нет
    bool ready = false; // true, когда колбэк сработал — можно обрабатывать результат
    bool ok = false; // true — пользователь выбрал файл; false — нажал «Отмена»
    std::string path; // путь к выбранному файлу (валиден только при ok == true)

    void reset() noexcept;
};

// Состояние диалога сохранения файла
struct SaveDialogState
{
    bool pending = false; // true, пока диалог сохранения открыт
    bool ready = false; // true, когда колбэк сработал — можно обрабатывать результат
    bool ok = false; // true — пользователь задал имя файла; false — отмена
    std::string path; // путь, куда сохранять (валиден только при ok == true)

    void reset() noexcept;
};

// ---- Callback-методы для диалоговых окон ----

void SDLCALL onFileDialogResult(void* userdata, const char* const* filelist, int /*filter*/);

void SDLCALL onSaveFileDialogResult(void* userdata, const char* const* filelist, int /*filter*/);

#endif // ! __DIALOG_WINDOWS_IMD_MAR_VIK__
