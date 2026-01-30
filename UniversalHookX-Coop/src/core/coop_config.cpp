#include "coop_config.hpp"

#include <Windows.h>

#include <cctype>
#include <fstream>
#include <string>

#include "../console/console.hpp"
#include "../utils/utils.hpp"

namespace {
	CoopConfig::Settings g_settings = { 'I', 'J', 'K', 'L', 'T', VK_F1, true };
	bool g_loaded = false;

	std::string ReadTextFile(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in) {
			return {};
		}
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(static_cast<size_t>(in.tellg()));
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		return contents;
	}

	std::string GetModuleDir( ) {
		char path[MAX_PATH] = {};
		GetModuleFileNameA(U::GetCurrentImageBase( ), path, MAX_PATH);
		std::string full(path);
		size_t pos = full.find_last_of("\\/");
		if (pos == std::string::npos) {
			return ".";
		}
		return full.substr(0, pos);
	}

	std::string GetExeDir( ) {
		char path[MAX_PATH] = {};
		GetModuleFileNameA(NULL, path, MAX_PATH);
		std::string full(path);
		size_t pos = full.find_last_of("\\/");
		if (pos == std::string::npos) {
			return ".";
		}
		return full.substr(0, pos);
	}

	std::string JoinPath(const std::string& base, const std::string& rel) {
		if (base.empty()) {
			return rel;
		}
		if (rel.empty()) {
			return base;
		}
		std::string out = base;
		if (out.back() != '\\' && out.back() != '/') {
			out.push_back('\\');
		}
		if (rel.rfind(".\\", 0) == 0) {
			out += rel.substr(2);
		} else if (rel.rfind("./", 0) == 0) {
			out += rel.substr(2);
		} else {
			out += rel;
		}
		return out;
	}

	bool FindJsonString(const std::string& json, const char* key, std::string& out) {
		std::string needle = std::string("\"") + key + "\"";
		size_t pos = json.find(needle);
		if (pos == std::string::npos) {
			return false;
		}
		pos = json.find(':', pos + needle.size());
		if (pos == std::string::npos) {
			return false;
		}
		pos++;
		while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
			++pos;
		}
		if (pos >= json.size() || json[pos] != '"') {
			return false;
		}
		++pos;
		std::string value;
		while (pos < json.size()) {
			char c = json[pos++];
			if (c == '\\' && pos < json.size()) {
				char esc = json[pos++];
				switch (esc) {
					case '"':
					case '\\':
					case '/':
						value.push_back(esc);
						break;
					case 'n':
						value.push_back('\n');
						break;
					case 't':
						value.push_back('\t');
						break;
					case 'r':
						value.push_back('\r');
						break;
					default:
						value.push_back(esc);
						break;
				}
				continue;
			}
			if (c == '"') {
				break;
			}
			value.push_back(c);
		}
		out = value;
		return true;
	}

	bool FindJsonBool(const std::string& json, const char* key, bool& out) {
		std::string needle = std::string("\"") + key + "\"";
		size_t pos = json.find(needle);
		if (pos == std::string::npos) {
			return false;
		}
		pos = json.find(':', pos + needle.size());
		if (pos == std::string::npos) {
			return false;
		}
		pos++;
		while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
			++pos;
		}
		if (json.compare(pos, 4, "true") == 0) {
			out = true;
			return true;
		}
		if (json.compare(pos, 5, "false") == 0) {
			out = false;
			return true;
		}
		return false;
	}

	std::string ToUpperTrim(std::string s) {
		size_t start = 0;
		while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
			++start;
		}
		size_t end = s.size();
		while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
			--end;
		}
		std::string out = s.substr(start, end - start);
		for (char& c : out) {
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		}
		return out;
	}

	int ParseKey(const std::string& raw, int fallback) {
		std::string s = ToUpperTrim(raw);
		if (s.empty()) {
			return fallback;
		}
		if (s.size() == 1) {
			return static_cast<int>(s[0]);
		}
		if (s == "UP" || s == "VK_UP") return VK_UP;
		if (s == "DOWN" || s == "VK_DOWN") return VK_DOWN;
		if (s == "LEFT" || s == "VK_LEFT") return VK_LEFT;
		if (s == "RIGHT" || s == "VK_RIGHT") return VK_RIGHT;
		if (s == "ESC" || s == "ESCAPE" || s == "VK_ESCAPE") return VK_ESCAPE;
		if (s == "SPACE" || s == "VK_SPACE") return VK_SPACE;
		if (s == "TAB" || s == "VK_TAB") return VK_TAB;
		if (s == "ENTER" || s == "RETURN" || s == "VK_RETURN") return VK_RETURN;
		if (s == "SHIFT" || s == "VK_SHIFT") return VK_SHIFT;
		if (s == "CTRL" || s == "CONTROL" || s == "VK_CONTROL") return VK_CONTROL;
		if (s == "ALT" || s == "VK_MENU") return VK_MENU;
		if (s[0] == 'F' && s.size() <= 3) {
			int n = 0;
			for (size_t i = 1; i < s.size(); ++i) {
				if (s[i] < '0' || s[i] > '9') {
					n = 0;
					break;
				}
				n = (n * 10) + (s[i] - '0');
			}
			if (n >= 1 && n <= 24) {
				return VK_F1 + (n - 1);
			}
		}
		if (s.rfind("0X", 0) == 0 && s.size() > 2) {
			return static_cast<int>(std::strtoul(s.c_str(), nullptr, 16));
		}
		bool all_digits = true;
		for (char c : s) {
			if (!std::isdigit(static_cast<unsigned char>(c))) {
				all_digits = false;
				break;
			}
		}
		if (all_digits) {
			return static_cast<int>(std::strtoul(s.c_str(), nullptr, 10));
		}
		return fallback;
	}

	void LoadFromJson(const std::string& json) {
		std::string val;
		if (FindJsonString(json, "up", val)) {
			g_settings.key_up = ParseKey(val, g_settings.key_up);
		}
		if (FindJsonString(json, "left", val)) {
			g_settings.key_left = ParseKey(val, g_settings.key_left);
		}
		if (FindJsonString(json, "down", val)) {
			g_settings.key_down = ParseKey(val, g_settings.key_down);
		}
		if (FindJsonString(json, "right", val)) {
			g_settings.key_right = ParseKey(val, g_settings.key_right);
		}
		if (FindJsonString(json, "attack", val)) {
			g_settings.key_attack = ParseKey(val, g_settings.key_attack);
		}
		if (FindJsonString(json, "menu", val)) {
			g_settings.key_menu = ParseKey(val, g_settings.key_menu);
		}
		bool show_console = g_settings.show_console;
		if (FindJsonBool(json, "show_console", show_console)) {
			g_settings.show_console = show_console;
		}
	}

	void LoadConfig( ) {
		g_settings = { 'I', 'J', 'K', 'L', 'T', VK_F1, true };

		const std::string filename = "uhx_coop_config.json";
		const std::string modulePath = JoinPath(GetModuleDir( ), filename);
		std::string json = ReadTextFile(modulePath);
		if (json.empty()) {
			const std::string exePath = JoinPath(GetExeDir( ), filename);
			json = ReadTextFile(exePath);
		}

		if (json.empty()) {
			LOG("[!] Coop config not found (using defaults).\n");
			return;
		}

		LoadFromJson(json);
	}
} // namespace

namespace CoopConfig {
	const Settings& Get( ) {
		if (!g_loaded) {
			LoadConfig( );
			g_loaded = true;
		}
		return g_settings;
	}

	void Reload( ) {
		LoadConfig( );
		g_loaded = true;
	}
} // namespace CoopConfig
