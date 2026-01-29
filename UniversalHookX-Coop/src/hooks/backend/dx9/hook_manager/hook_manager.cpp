#include "hook_manager.hpp"

#include "../../../../console/console.hpp"
#include "../../../../dependencies/minhook/MinHook.h"
#include "../../../p2_input.hpp"

BitBlt_t oBitBlt = nullptr;
RGSSEval_t OriginalRGSSEval = nullptr;
RGSSGetInt_t OriginalRGSSGetInt = nullptr;
RGSSGetDouble_t OriginalRGSSGetDouble = nullptr;
RGSSGetTable_t OriginalRGSSGetTable = nullptr;
RGSSGetStringUTF8_t OriginalRGSSGetStringUTF8 = nullptr;
CreateCompatibleDC_t OriginalCreateCompatibleDC = nullptr;

namespace {
	bool g_rgss_hooked = false;

	void TryHookRGSS( ) {
		if (g_rgss_hooked) {
			return;
		}

		HMODULE rgssModule = GetModuleHandleA("RGSS301.dll");
		if (!rgssModule) {
			return;
		}

		void* rgssEvalAddr = reinterpret_cast<void*>(0x10003760);
		MH_STATUS createStatus = MH_CreateHook(rgssEvalAddr, &HookedRGSSEval, reinterpret_cast<void**>(&OriginalRGSSEval));
		if (createStatus == MH_OK || createStatus == MH_ERROR_ALREADY_CREATED) {
			MH_STATUS enableStatus = MH_EnableHook(rgssEvalAddr);
			if (enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED) {
				g_rgss_hooked = true;
				LOG("[+] RGSS hook installed.\n");
			}
		}
	}
}

BOOL WINAPI hkBitBlt(HDC hdcDest,
                     int x,
                     int y,
                     int cx,
                     int cy,
                     HDC hdcSrc,
                     int x1,
                     int y1,
                     DWORD rop) {
	TryHookRGSS( );
	P2Input::Tick( );
	return oBitBlt(hdcDest, x, y, cx, cy, hdcSrc, x1, y1, rop);
}

void HookedRGSSEval(const char* ruby_code) {
	OriginalRGSSEval(ruby_code);
}

int HookedRGSSGetInt(const char* ruby_code) {
	return OriginalRGSSGetInt ? OriginalRGSSGetInt(ruby_code) : 0;
}

double HookedRGSSGetDouble(const char* ruby_code) {
	return OriginalRGSSGetDouble ? OriginalRGSSGetDouble(ruby_code) : 0.0;
}

void HookedRGSSGetTable(void* buffer, void* ruby_object) {
	if (OriginalRGSSGetTable) {
		OriginalRGSSGetTable(buffer, ruby_object);
	}
}

const char* HookedRGSSGetStringUTF8(const char* ruby_code) {
	return OriginalRGSSGetStringUTF8 ? OriginalRGSSGetStringUTF8(ruby_code) : nullptr;
}

HDC WINAPI hkCreateCompatibleDC(HDC hdc) {
	return OriginalCreateCompatibleDC ? OriginalCreateCompatibleDC(hdc) : nullptr;
}

int __stdcall HookedMessageHandling(DWORD, DWORD, DWORD) {
	return 0;
}

void HookGDI( ) {
	if (MH_CreateHookApi(L"gdi32", "BitBlt", &hkBitBlt, reinterpret_cast<void**>(&oBitBlt)) != MH_OK) {
		LOG("[!] Failed to hook BitBlt.\n");
	}

	TryHookRGSS( );
	MH_EnableHook(MH_ALL_HOOKS);
}

void Hook_kglBindFramebuffer( ) {
	// Not used in coop build.
}
