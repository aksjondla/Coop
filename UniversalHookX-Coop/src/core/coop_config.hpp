#pragma once

namespace CoopConfig {
	struct Settings {
		int key_up;
		int key_left;
		int key_down;
		int key_right;
		int key_attack;
		int key_menu;
		bool show_console;
	};

	const Settings& Get( );
	void Reload( );
} // namespace CoopConfig
