#pragma comment(lib, "user32.lib")

#include <Windows.h>

#include <thread>
#include <stdexcept>
#include <format>
#include <print>

namespace hax {
	void attach_console() {
		if (!AllocConsole()) {
			throw std::runtime_error("Failed to allocate console.");
		}

		static FILE* file;
		freopen_s(&file, "CONOUT$", "w", stdout);

		if (!SetConsoleTitle(std::format("rsblox/local_rcc @ {}", __TIMESTAMP__).c_str())) {
			throw std::runtime_error("Failed to set console title.");
		}
	}
}

LONG exception_filter(EXCEPTION_POINTERS* exception_info) {
	MessageBox(
		NULL,
		"An unexpected error occurred and Rsblox needs to quit.  We're sorry!",
		"Rsblox Crash",
		MB_OK
	);

	exit(1); // TODO(7ap): Copy exception info to clipboard for analysis.

	return EXCEPTION_EXECUTE_HANDLER;
}

void thread() {
	SetUnhandledExceptionFilter(exception_filter);

	hax::attach_console();
	std::println("Hello, world!");

	// ...
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
	DisableThreadLibraryCalls(module);

	if (reason == DLL_PROCESS_ATTACH) {
		std::thread(thread).detach();
	}

	return TRUE;
}
