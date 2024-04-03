#include <Windows.h>
#include <Psapi.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <print>
#include <string>
#include <thread>
#include <vector>

#include <Luau/BytecodeBuilder.h>
#include <Luau/BytecodeUtils.h>
#include <Luau/Compiler.h>
#include <Pattern16.h>
#include <safetyhook.hpp>
#include <xxhash.h>
#include <zstd.h>

const HMODULE module = GetModuleHandle(NULL);

void* operator new(size_t size) {
	return reinterpret_cast<void* (*)(size_t size)>(GetProcAddress(module, "rbxAllocate"))(size);
}

void operator delete(void* ptr) noexcept {
	reinterpret_cast<void (*)(void* ptr)>(GetProcAddress(module, "rbxDeallocate"))(ptr);
}

namespace types {
	typedef std::string (*compile)(const std::string& source, int target, int options);
	typedef void* (*deserialize_item)(void* self, void* result, void* in_bitstream, int item_type);
	enum item_type { client_qos = 0x1f };
	enum network_value_format { protected_string_bytecode = 0x2f };
}

namespace hooks {
	SafetyHookInline compile {};
	SafetyHookInline deserialize_item {};
}

types::compile compile;

std::string compile_hook(const std::string& source, int target, int options) {
	std::println(
		"LuaVM::compile(source: {}, target: {}, options: {})",
		"...", target, options
	);

	class bytecode_encoder_client : public Luau::BytecodeEncoder {
		void encode(uint32_t* data, size_t count) override {
			for (size_t i = 0; i < count;) {
				uint8_t op = LUAU_INSN_OP(data[i]);

				int oplen = Luau::getOpLength(LuauOpcode(op));
				uint8_t openc = uint8_t(op * 227);
				data[i] = openc | (data[i] & ~0xff);

				i += oplen;
			}
		}
	};

	bytecode_encoder_client encoder {};

	const char* special_globals[] = {
		"game",
		"Game",
		"workspace",
		"Workspace",
		"script",
		"shared",
		"plugin",
		nullptr
	};

	std::string bytecode = Luau::compile(source, {2, 1, 0, "Vector3", "new", nullptr, special_globals}, {}, &encoder);

	size_t compressed_bytecode_capacity = ZSTD_compressBound(bytecode.length());
	uint32_t uncompressed_bytecode_size = static_cast<uint32_t>(bytecode.length());

	std::vector<std::uint8_t> encoded_bytecode(
		// Hash + Size + Compressed Bytecode
		4 + 4 + compressed_bytecode_capacity
	);

	std::memcpy(&encoded_bytecode[0], "RSB1", 4);
	std::memcpy(&encoded_bytecode[4], &uncompressed_bytecode_size, 4);

	size_t compressed_bytecode_size = ZSTD_compress(
		&encoded_bytecode[8],
		compressed_bytecode_capacity,
		bytecode.c_str(),
		bytecode.length(),
		ZSTD_maxCLevel()
	);

	size_t encoded_bytecode_size = 4 + 4 + compressed_bytecode_size;
	XXH32_hash_t hash = XXH32(encoded_bytecode.data(), encoded_bytecode_size, 42);
	std::uint8_t key[4]; std::memcpy(key, &hash, 4);

	for (size_t i = 0; i < encoded_bytecode_size; i++) {
		encoded_bytecode[i] ^= (key[i % 4]) + (i * 41);
	}

	return std::string(reinterpret_cast<char*>(encoded_bytecode.data()), encoded_bytecode_size);
}

types::deserialize_item deserialize_item;

void* deserialize_item_hook(void* self, void* deserialized_item, void* in_bitstream, int item_type) {
	std::println(
		"RBX::Network::Replicator::deserializeItem(this: {:p}, deserializedItem: {:p}, inBitstream: {:p}, itemType: {:x})",
		self, deserialized_item, in_bitstream, item_type
	);

	if (item_type == types::item_type::client_qos) {
		std::memset(deserialized_item, 0, 16);
		return deserialized_item;
	}

	return hooks::deserialize_item.call<void*>(self, deserialized_item, in_bitstream, item_type);
}

uintptr_t type_for_property_mov_imm_address;

void patch_type_for_property() {
	std::byte byte = std::byte(types::network_value_format::protected_string_bytecode);

	DWORD old_protect;
	VirtualProtect(reinterpret_cast<void*>(type_for_property_mov_imm_address), 16, PAGE_EXECUTE_READWRITE, &old_protect);
	*reinterpret_cast<std::byte*>(type_for_property_mov_imm_address) = byte;
	VirtualProtect(reinterpret_cast<void*>(type_for_property_mov_imm_address), 16, old_protect, &old_protect);
}

void attach_console() {
	#if defined(_WIN64)
	AllocConsole();
	static FILE* file; freopen_s(&file, "CONOUT$", "w", stdout);
	SetConsoleTitleA(std::format("rsblox/local_rcc @ {}", __TIMESTAMP__).c_str());
	#endif
}

void* pattern_scan(const std::string& signature) {
    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo));

    return Pattern16::scan(reinterpret_cast<void*>(module), moduleInfo.SizeOfImage, signature);
}

bool scan_for_addresses() {
	#define SCAN_WITH_OFFSET(symbol, patternarg, offsetFromResult) {  		\
        std::string pattern = (patternarg);                                 \
        symbol = (decltype(symbol))pattern_scan(pattern);                   \
        if (!symbol) return false;                                          \
        symbol = (decltype(symbol))((uintptr_t)symbol + offsetFromResult);  \
        std::println("Found " #symbol);                                     \
    }

    #define SCAN(symbol, patternarg) SCAN_WITH_OFFSET(symbol, patternarg, 0)

	#if defined(_WIN64) && defined(_M_X64)
	SCAN(compile, "33 C0 48 C7 41 18 0F 00 00 00 48 89 01 48 89 41 10 88 01 48 8B C1");
	#elif defined(__APPLE__) && defined(__x86_64__)
	SCAN(compile, "55 48 89 E5 53 50 48 89 FB 48 8D 35 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 89 D8 48 83 C4 08 5B 5D C3");
	#elif defined(__APPLE__) && defined(__aarch64__)
	#error TODO
	#endif
	
	#if defined(_WIN64) && defined(_M_X64)
	SCAN(deserialize_item, "48 89 5C 24 ?? 48 89 74 24 ?? 48 89 54 24 ?? 55 57 41 56 48 8B EC 48 83 EC 40 49 8B F8");
	#elif defined(__APPLE__) && defined(__x86_64__)
	SCAN(deserialize_item, "55 48 89 E5 41 57 41 56 53 50 48 89 FB 48 C7 07 00 00 00 00 FF C9 83 F9 15");
	#elif defined(__APPLE__) && defined(__aarch64__)
	#error TODO
	#endif

	#if defined(_WIN64) && defined(_M_X64)
	SCAN_WITH_OFFSET(type_for_property_mov_imm_address, "C6 44 24 ?? 06 EB ?? 48 3B 15", 4);
	#elif defined(__APPLE__) && defined(__x86_64__)
	SCAN_WITH_OFFSET(type_for_property_mov_imm_address, "E8 ?? ?? ?? ?? 84 C0 B9 2F 00 00 00 B8 06 00 00 00 48 0F 45 C1", 13);
	#elif defined(__APPLE__) && defined(__aarch64__)
	#error TODO
	#endif

	#undef SCAN_WITH_OFFSET
	#undef SCAN

	return true;
}

void thread() {
	attach_console(); std::println("Hello, world!");

	if (!scan_for_addresses()) {
		std::println(stderr, "[ERROR] Could not find all needed patterns. local_rcc must be updated.");
		return;
	}
	std::println("Scanning finished.");

	hooks::compile = safetyhook::create_inline(compile, compile_hook);
	std::println("Hooked LuaVM::compile.");

	hooks::deserialize_item = safetyhook::create_inline(deserialize_item, deserialize_item_hook);
	std::println("Hooked RBX::Network::Replicator::deserializeItem.");

	patch_type_for_property();
	std::println("Patched RBX::Network::NetworkSchema::typeForProperty.");

	std::println("Made with <3 by 7ap & Epix @ https://github.com/rsblox - come join us!");
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
	DisableThreadLibraryCalls(module);

	if (reason == DLL_PROCESS_ATTACH) {
		std::thread(thread).detach();
	}

	return TRUE;
}
