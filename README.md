# 🏠 local_rcc

*Selfhosted game servers for the latest versions of Roblox.*


Current targeted Roblox Studio deployment: `version-1c901af996da417b` (LIVE, v617) - [Download with RDD](https://rdd.latte.to/?channel=LIVE&binaryType=WindowsStudio64&version=version-1c901af996da417b)

## 📖 Explanation

There are four steps to make a team test compatible with Player:
[setting a fast flag](#debuglocalrccserverconnection),
[blocking an unhandled item](#clientqos),
[compiling compatible bytecode](#bytecodeencoderclient),
and [replicating the bytecode](#networkschema).

### DebugLocalRccServerConnection

Overriding the fast flag, `DebugLocalRccServerConnection`, on both Player and
Studio will disable almost all the network security required for joining games
on the server and client. It also will force Player to connect to `localhost`.

With this alone, you can already join a Studio team test from Player - however,
it is not _playable_ yet.

### ClientQoS

<!-- 👀 -->

Shortly after Player joins the team test server, it will be kicked after
sending a `ClientQoS` item - which is unhandled by Studio. This can be resolved
by discarding the result of `RBX::Network::Replicator::deserializeItem` if an
item type of `ClientQoS` is passed.

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
