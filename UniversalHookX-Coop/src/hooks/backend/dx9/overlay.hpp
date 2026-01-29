#ifndef OVERLAY_HPP
#define OVERLAY_HPP

#include <Windows.h>

namespace Overlay {
    void StartOverlay(HWND targetWindow);
    void StopOverlay( );
    DWORD WINAPI OverlayThread(LPVOID lpParam);
    LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void RenderFrame( );           // Публичный метод для ручного рендеринга
    void UpdateOverlayPosition( ); // Для ручного обновления позиции
} // namespace Overlay

#endif // OVERLAY_HPP
