# 🏠 local_rcc

*Selfhosted game servers for Roblox **versions ≤623**.*

**local_rcc** was a library that worked with Roblox Studio (Windows only)
and allowed the Roblox Player to connect to a Roblox Studio hosted server.

It is now no longer effective ([link](https://github.com/rsblox/local_rcc/issues/16#issuecomment-2102678689)).

## 🛠️ Building

This guide is for building on Microsoft Windows.

Make sure you have the following prerequisites:

- [Microsoft Visual 2022 with the base C++ development pack](https://visualstudio.microsoft.com/vs/features/cplusplus/)
    - Includes necessary compiler and linker, plus a build of CMake
- [Ninja build system](https://ninja-build.org/)
    - Easily install with `winget install Ninja-build.ninja`

---

Clone the repository:
```
git clone https://github.com/rsblox/local_rcc.git
```

There are two methods for building:

1. Visual Studio Code with the CMake Tools extension
2. Visual Studio's built-in CMake features

### With Visual Studio Code & CMake Tools extension

Make sure you have [Visual Studio Code](https://code.visualstudio.com/)
installed with the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
extention installed.

Open the cloned repository:
```
code local_rcc
```

If prompted to select a build kit, select the `amd64` one.

CMake Tools will automatically configure the project; be patient and wait in
the output for configuration to complete; this includes the fetching of
dependencies from the Internet.

Navigate to the CMake tab and set the build type to `RelWithDebInfo`.

In the project outline in the same CMake tab, build the local_rcc
[local_rcc.dll] target. Once completed, the library should be located at
`bin/local_rcc.dll`.

### With Visual Studio

Open Visual Studio 2022 and navigate to `File -> Open -> CMake...` and navigate
to the `CMakeLists.txt` file in the repository cloned earlier. Open the Output
and watch for fetching the dependencies and configuring to complete. This may
take a bit.

Make sure the configuration on the top is set to `x64-RelWithDebInfo`. In the
Solution Explorer window, click the icon with the caption "Switch between
solutions and available views". Click on the CMake Targets view. This list
should be populated once CMake configuration is successful. Right click on the
local_rcc target, and click "Build local_rcc". The resultant build should be
located at `out/build/x64-RelWithDebInfo/local_rcc.dll`.

## 📖 Explanation

There are four steps to make a team test compatible with Player:
[setting a fast flag](#debuglocalrccserverconnection),
[blocking an unhandled item](#clientqositem),
[compiling compatible bytecode](#bytecodeencoderclient),
and [replicating the bytecode](#networkschema).

### DebugLocalRccServerConnection

Overriding the fast flag, `DebugLocalRccServerConnection`, on both Player and
Studio will allow a connection to be made between the client and the server.
It also will force the client to connect to `localhost|53640`.

With this alone, you can already join a Studio team test from Player - however,
it is not _playable_ yet.

### ClientQoSItem

<!-- 👀 -->

Shortly after Player joins the team test server, it will be kicked after
sending a `ClientQoSItem` - which is unhandled by Studio. This can be resolved
by refusing to deserialize the item if a `ClientQoSItem` is passed to
`RBX::Network::Replicator::deserializeItem`.

### BytecodeEncoderClient

Opening the developer console in Player after connecting to the team test
server shows some strange errors with the path of `LocalScript` instances that
are replicated to Player and none of them are executed. This is partly due to
how Studio populates the contents of the server's `legalScripts` vector. A
`LegalScript` is a 'whitelisted' script for replication, containing source and
bytecode. The function `RBX::Network::Server::registerLegalSharedScript` is
used to register _and compile_ these scripts.

Studio already calls this function and even invokes `LuaVM::compile` - however,
on Studio it just returns an empty `std::string`. This can be fixed by hooking
`LuaVM::compile` and compiling source with `LuaVM::BytecodeEncoderClient`.

### NetworkSchema

Even after compiling scripts correctly and populating each `LegalScript` with
bytecode, the same error appears in the developer console. When a server is
started, a `NetworkSchema` is generated, which contains info for every class in
the reflection database. This includes info on how to send properties, like
`Script.Source`, which is determined by their `NetworkValueFormat`. By default,
Studio selects the type for `Script.Source` to be `ProtectedStringSource`, and
sends the source code of scripts to other Studio clients for them to compile
themselves.

This can be fixed by forcing the server to use `ProtectedStringBytecode`, which
will make it get the stored bytecode from the `legalScripts` vector and send it
over when serializing the property. The generation of the `NetworkSchema`
occurs in `RBX::Network::NetworkSchema::generateSchemaDefinitionPacket`, and
the portion of interest is when it chooses the corresponding
`NetworkValueFormat` for the reflection type `ProtectedString`.
