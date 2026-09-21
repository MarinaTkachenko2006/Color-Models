#include "dialog_windows.h"

void FileDialogState::reset() noexcept { ready = false; ok = false; path.clear(); }
void SaveDialogState::reset() noexcept { ready = false; ok = false; path.clear(); }

void SDLCALL onFileDialogResult(void* userdata, const char* const* filelist, int /*filter*/) {
    auto* st = static_cast<FileDialogState*>(userdata);

    // Диалог закрыт, результат готов к обработке главным циклом
    st->pending = false;
    st->ready = true;

    // Файл не выбран, т.е. пользователь нажал "отмена"
    if (!filelist || !filelist[0]) { st->ok = false; return; }

    // Файл выбран
    st->ok = true;
    st->path = filelist[0];
}

void SDLCALL onSaveFileDialogResult(void* userdata, const char* const* filelist, int /*filter*/) {
    auto* st = static_cast<SaveDialogState*>(userdata);

    // Диалог закрыт, результат готов к обработке
    st->pending = false;
    st->ready = true;

    // Пользователь не задал путь, т.е. пользователь нажал "отмена"
    if (!filelist || !filelist[0]) { st->ok = false; return; }

    // Путь получен
    st->ok = true;
    st->path = filelist[0];
}
