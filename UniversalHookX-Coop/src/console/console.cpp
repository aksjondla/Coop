#include <Windows.h>

#include <fstream>

#include "console.hpp"

#include "../hooks/hooks.hpp"

namespace {
	bool g_console_allocated = false;
}

void Console::Alloc( ) {
#ifndef DISABLE_LOGGING_CONSOLE
	if (g_console_allocated) {
		return;
	}
	AllocConsole( );

	SetConsoleTitleA("UniversalHookX - Debug Console");

	freopen_s(reinterpret_cast<FILE**>(stdin), "conin$", "r", stdin);
	freopen_s(reinterpret_cast<FILE**>(stdout), "conout$", "w", stdout);

	::ShowWindow(GetConsoleWindow( ), SW_SHOW);
	g_console_allocated = true;
#endif
}

void Console::Free( ) {
#ifndef DISABLE_LOGGING_CONSOLE
	if (!g_console_allocated) {
		return;
	}
	fclose(stdin);
	fclose(stdout);

	if (H::bShuttingDown) {
		::ShowWindow(GetConsoleWindow( ), SW_HIDE);
	} else {
		FreeConsole( );
	}
	g_console_allocated = false;
#endif
}
