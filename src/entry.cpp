#include <Windows.h>

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

namespace addrs {
	const uintptr_t compile = 0x1a8e660;
	const uintptr_t deserialize_item = 0x16684c0;
	const uintptr_t generate_schema_definition_packet = 0x16fc80a;
}

namespace types {
	typedef std::string (*compile)(const std::string& source, int target, int options);
	typedef void* (*deserialize_item)(void* self, void* result, void* in_bitstream, int item_type);
	enum item_type { client_qos = 0x1f };
	enum network_value_format { proteted_string_bytecode = 0x2f };
}

namespace hooks {
	SafetyHookInline compile {};
	SafetyHookInline deserialize_item {};
}

types::compile compile = reinterpret_cast<types::compile>(reinterpret_cast<uintptr_t>(module) + addrs::compile);

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

types::deserialize_item deserialize_item = reinterpret_cast<types::deserialize_item>(reinterpret_cast<uintptr_t>(module) + addrs::deserialize_item);

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

void patch_generate_schema_definition_packet() {
	uintptr_t addr = reinterpret_cast<uintptr_t>(module) + addrs::generate_schema_definition_packet;
	std::byte byte = std::byte(types::network_value_format::proteted_string_bytecode);

	DWORD old_protect;
	VirtualProtect(reinterpret_cast<void*>(addr), 16, PAGE_EXECUTE_READWRITE, &old_protect);
	*reinterpret_cast<std::byte*>(addr + 4) = byte;
	VirtualProtect(reinterpret_cast<void*>(addr), 16, old_protect, &old_protect);
}

void attach_console() {
	AllocConsole();
	static FILE* file; freopen_s(&file, "CONOUT$", "w", stdout);
	SetConsoleTitle(std::format("rsblox/local_rcc @ {}", __TIMESTAMP__).c_str());
}

void thread() {
	attach_console(); std::println("Hello, world!");

	hooks::compile = safetyhook::create_inline(compile, compile_hook);
	std::println("Hooked LuaVM::compile.");

	hooks::deserialize_item = safetyhook::create_inline(deserialize_item, deserialize_item_hook);
	std::println("Hooked RBX::Network::Replicator::deserializeItem.");

	patch_generate_schema_definition_packet();
	std::println("Patched RBX::Network::NetworkSchema::generateSchemaDefinitionPacket.");

	std::println("Made with <3 by 7ap & Epix @ https://github.com/rsblox - come join us!");
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
	DisableThreadLibraryCalls(module);

	if (reason == DLL_PROCESS_ATTACH) {
		std::thread(thread).detach();
	}

	return TRUE;
}
