#define _CRT_SECURE_NO_WARNINGS

#include "overlay.hpp"

#include "../../../console/console.hpp"
#include "../ImDui/imdui.h"

#include <d2d1.h>
#include <dwrite.h>
#include <tchar.h>
#include <windowsx.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace {
	HWND hwndGame = NULL;
	HWND hwndOverlay = NULL;
	HANDLE hOverlayThread = NULL;
	bool bOverlayRunning = true;
	bool g_menuOpen = false;

	ID2D1Factory* pD2DFactory = NULL;
	ID2D1DCRenderTarget* pRenderTarget = NULL;
	ID2D1SolidColorBrush* pBrush = NULL;
	IDWriteFactory* pDWriteFactory = NULL;

	HDC g_memDC = NULL;
	HBITMAP g_memBitmap = NULL;
	HBITMAP g_oldBitmap = NULL;
	void* g_bitmapBits = NULL;
	int g_surfaceWidth = 0;
	int g_surfaceHeight = 0;

	bool CreateOverlaySurface(int width, int height) {
		if (width <= 0 || height <= 0) {
			return false;
		}

		if (!g_memDC) {
			g_memDC = CreateCompatibleDC(NULL);
			if (!g_memDC) {
				LOG("[!] Failed to create memory DC.\n");
				return false;
			}
		}

		if (g_memBitmap && width == g_surfaceWidth && height == g_surfaceHeight) {
			return true;
		}

		if (g_memBitmap) {
			SelectObject(g_memDC, g_oldBitmap);
			DeleteObject(g_memBitmap);
			g_memBitmap = NULL;
			g_oldBitmap = NULL;
			g_bitmapBits = NULL;
		}

		BITMAPINFO bmi = { };
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = width;
		bmi.bmiHeader.biHeight = -height; // top-down
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		g_memBitmap = CreateDIBSection(g_memDC, &bmi, DIB_RGB_COLORS, &g_bitmapBits, NULL, 0);
		if (!g_memBitmap) {
			LOG("[!] Failed to create DIB section.\n");
			return false;
		}

		g_oldBitmap = (HBITMAP)SelectObject(g_memDC, g_memBitmap);
		g_surfaceWidth = width;
		g_surfaceHeight = height;
		return true;
	}

	bool InitDirect2D(HWND hWnd, int width, int height) {
		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
		if (FAILED(hr)) {
			LOG("[!] Failed to create Direct2D factory.\n");
			return false;
		}

		hr = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&pDWriteFactory));
		if (FAILED(hr)) {
			LOG("[!] Failed to create DirectWrite factory.\n");
			return false;
		}

		D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
		hr = pD2DFactory->CreateDCRenderTarget(&rtProps, &pRenderTarget);
		if (FAILED(hr)) {
			LOG("[!] Failed to create render target.\n");
			return false;
		}

		if (!CreateOverlaySurface(width, height)) {
			LOG("[!] Failed to create overlay surface.\n");
			return false;
		}

		hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &pBrush);
		if (FAILED(hr)) {
			LOG("[!] Failed to create brush.\n");
			return false;
		}

		ImDui::InitResources(pD2DFactory, pDWriteFactory, NULL, pRenderTarget);
		LOG("[+] ImDui initialized.\n");
		return true;
	}

	void CleanupDirect2D( ) {
		if (pBrush) pBrush->Release( );
		if (pRenderTarget) pRenderTarget->Release( );
		if (pD2DFactory) pD2DFactory->Release( );
		if (pDWriteFactory) pDWriteFactory->Release( );

		if (g_memDC) {
			if (g_memBitmap) {
				SelectObject(g_memDC, g_oldBitmap);
				DeleteObject(g_memBitmap);
				g_memBitmap = NULL;
				g_oldBitmap = NULL;
				g_bitmapBits = NULL;
			}
			DeleteDC(g_memDC);
			g_memDC = NULL;
			g_surfaceWidth = 0;
			g_surfaceHeight = 0;
		}

		ImDui::Shutdown( );
		LOG("[+] ImDui shutdown.\n");
	}
} // namespace

void Overlay::UpdateOverlayPosition( ) {
	if (!hwndGame || !hwndOverlay) {
		return;
	}

	RECT gameRect;
	GetWindowRect(hwndGame, &gameRect);
	int width = gameRect.right - gameRect.left;
	int height = gameRect.bottom - gameRect.top;

	SetWindowPos(hwndOverlay, HWND_TOPMOST, gameRect.left, gameRect.top, width, height,
	             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);

	if (width != g_surfaceWidth || height != g_surfaceHeight) {
		CreateOverlaySurface(width, height);
	}
}

void Overlay::RenderFrame( ) {
	if (!pRenderTarget || !hwndOverlay) {
		return;
	}

	if (!CreateOverlaySurface(g_surfaceWidth, g_surfaceHeight)) {
		return;
	}

	RECT rc = { 0, 0, g_surfaceWidth, g_surfaceHeight };
	pRenderTarget->BindDC(g_memDC, &rc);
	pRenderTarget->BeginDraw( );
	pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

	ImDui::NewFrame( );
	if (g_menuOpen) {
		bool show = true;
		ImDui::BeginWindow("UHX Coop", &show, ImFloat2(20, 20), ImFloat2(280, 120));
		ImDui::Text("Coop active");
		ImDui::Text("P2: I/J/K/L + T");
		ImDui::Text("F1: toggle UI");
		ImDui::EndWindow( );
	}
	ImDui::Render( );

	HRESULT hr = pRenderTarget->EndDraw( );
	if (FAILED(hr)) {
		return;
	}

	RECT wndRect;
	GetWindowRect(hwndOverlay, &wndRect);
	POINT ptDst = { wndRect.left, wndRect.top };
	POINT ptSrc = { 0, 0 };
	SIZE size = { g_surfaceWidth, g_surfaceHeight };
	BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
	UpdateLayeredWindow(hwndOverlay, NULL, &ptDst, &size, g_memDC, &ptSrc, 0, &blend, ULW_ALPHA);
}

LRESULT CALLBACK Overlay::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_ERASEBKGND:
			return 1;
		case WM_NCHITTEST:
			return g_menuOpen ? HTCLIENT : HTTRANSPARENT;
		case WM_MOUSEMOVE:
			ImDui::GetEvents( ).MousePos.x = GET_X_LPARAM(lParam);
			ImDui::GetEvents( ).MousePos.y = GET_Y_LPARAM(lParam);
			return 0;
		case WM_LBUTTONDOWN:
			ImDui::GetEvents( ).MouseDown = true;
			return 0;
		case WM_LBUTTONUP:
			ImDui::GetEvents( ).MouseDown = false;
			return 0;
		case WM_MOUSEWHEEL: {
			int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			ImDui::GetEvents( ).MouseWheel = (delta > 0) ? +1 : -1;
			return 0;
		}
		case WM_KEYDOWN:
			return 0;
		case WM_TIMER:
			UpdateOverlayPosition( );
			RenderFrame( );
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

DWORD WINAPI Overlay::OverlayThread(LPVOID) {
	RECT gameRect;
	GetWindowRect(hwndGame, &gameRect);
	int width = gameRect.right - gameRect.left;
	int height = gameRect.bottom - gameRect.top;

	WNDCLASS wc = { };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = TEXT("UHXCoopOverlay");
	RegisterClass(&wc);

	hwndOverlay = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
		TEXT("UHXCoopOverlay"), TEXT("UHX Coop Overlay"),
		WS_POPUP, gameRect.left, gameRect.top, width, height,
		NULL, NULL, GetModuleHandle(NULL), NULL);

	if (!hwndOverlay) {
		LOG("[!] Failed to create overlay window.\n");
		return 0;
	}

	ShowWindow(hwndOverlay, SW_SHOW);
	if (!InitDirect2D(hwndOverlay, width, height)) {
		LOG("[!] Failed to initialize Direct2D.\n");
		return 0;
	}

	SetTimer(hwndOverlay, 1, 16, NULL);

	MSG msg;
	bool prevF1 = false;
	while (bOverlayRunning) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		bool f1Down = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
		if (f1Down && !prevF1) {
			g_menuOpen = !g_menuOpen;
			SetWindowLong(hwndOverlay, GWL_EXSTYLE,
			              g_menuOpen ? (GetWindowLong(hwndOverlay, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT)
			                         : (GetWindowLong(hwndOverlay, GWL_EXSTYLE) | WS_EX_TRANSPARENT));
			SetWindowPos(hwndOverlay, NULL, 0, 0, 0, 0,
			             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
		}
		prevF1 = f1Down;
		Sleep(1);
	}

	KillTimer(hwndOverlay, 1);
	CleanupDirect2D( );
	return 0;
}

void Overlay::StartOverlay(HWND targetWindow) {
	hwndGame = targetWindow;
	if (!hwndGame) {
		MessageBox(NULL, TEXT("Окно игры не найдено!"), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
		return;
	}

	bOverlayRunning = true;
	hOverlayThread = CreateThread(NULL, 0, OverlayThread, NULL, 0, NULL);
	if (!hOverlayThread) {
		MessageBox(NULL, TEXT("Не удалось создать поток оверлея!"), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
	} else {
		LOG("[+] Overlay thread started.\n");
	}
}

void Overlay::StopOverlay( ) {
	if (hOverlayThread) {
		bOverlayRunning = false;
		WaitForSingleObject(hOverlayThread, INFINITE);
		CloseHandle(hOverlayThread);
		hOverlayThread = NULL;
	}

	if (hwndOverlay) {
		DestroyWindow(hwndOverlay);
		hwndOverlay = NULL;
	}
	LOG("[+] Overlay stopped.\n");
}
