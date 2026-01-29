#ifndef HOOK_MANAGER_HPP
#define HOOK_MANAGER_HPP

#include "../../../../console/console.hpp"
#include "../../../../dependencies/minhook/MinHook.h"
#include <Windows.h>

// Оригинальные функции и их прототипы
typedef BOOL(WINAPI* BitBlt_t)(HDC, int, int, int, int, HDC, int, int, DWORD);
typedef void (*RGSSEval_t)(const char*);
typedef int(__stdcall* RGSSGetInt_t)(const char*);
typedef double (*RGSSGetDouble_t)(const char*);
typedef void (*RGSSGetTable_t)(void*, void*);
typedef const char* (*RGSSGetStringUTF8_t)(const char*);
typedef HDC(WINAPI* CreateCompatibleDC_t)(HDC);

// Глобальные указатели на оригинальные функции
extern BitBlt_t oBitBlt;
extern RGSSEval_t OriginalRGSSEval;
extern RGSSGetInt_t OriginalRGSSGetInt;
extern RGSSGetDouble_t OriginalRGSSGetDouble;
extern RGSSGetTable_t OriginalRGSSGetTable;
extern RGSSGetStringUTF8_t OriginalRGSSGetStringUTF8;
extern CreateCompatibleDC_t OriginalCreateCompatibleDC;

// Основные функции для установки хуков
void HookGDI( );
void Hook_kglBindFramebuffer( );

// Хуки
BOOL WINAPI hkBitBlt(HDC, int, int, int, int, HDC, int, int, DWORD);
void HookedRGSSEval(const char*);
int HookedRGSSGetInt(const char*);
double HookedRGSSGetDouble(const char*);
void HookedRGSSGetTable(void*, void*);
const char* HookedRGSSGetStringUTF8(const char*);
HDC WINAPI hkCreateCompatibleDC(HDC);

// Пример обработки сообщения
int __stdcall HookedMessageHandling(DWORD, DWORD, DWORD);

#endif // HOOK_MANAGER_HPP
