#include <thread>

#include "hooks.hpp"

#include "../console/console.hpp"
#include "../dependencies/minhook/MinHook.h"
#include "../utils/utils.hpp"

#include "backend/dx9/hook_manager/hook_manager.hpp"
#include "backend/dx9/overlay.hpp"

namespace Hooks {
	void Init( ) {
		HWND hwnd = U::GetProcessWindow( );

		HookGDI( );
		Overlay::StartOverlay(hwnd);
	}

	void Free( ) {
		Overlay::StopOverlay( );
		MH_DisableHook(MH_ALL_HOOKS);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
