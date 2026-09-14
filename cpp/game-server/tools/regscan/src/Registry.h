#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "JavaScanner.h"
#include "Lexer.h"
#include "SourceScanner.h"

/** Collects all scans, runs the cross-file checks and produces the sorted registry model that the emitter turns into tables and a report. */
namespace aion::gameserver::tools::regscan {

struct Options {
	std::optional<std::filesystem::path> handlersRoot;            // include root that contains aion/gameserver/handlers
	std::optional<std::filesystem::path> clientPacketsRoot;       // include root that contains aion/gameserver/network/aion/clientpackets
	std::optional<std::filesystem::path> javaHandlers;            // game-server/data/handlers
	std::optional<std::filesystem::path> javaClientPacketFactory; // game-server/src/.../network/aion/AionClientPacketFactory.java
};

struct SourceFile {
	std::string file;    // display path
	std::string relPath; // relative to the root, forward slashes
	std::string content;
};

struct ScanInput {
	bool handlersGiven = false;
	std::vector<SourceFile> handlerFiles; // sorted by relPath
	bool clientPacketsGiven = false;
	std::vector<SourceFile> clientPacketFiles;
	bool javaHandlersGiven = false;
	std::vector<SourceFile> javaHandlerFiles;
	std::optional<SourceFile> javaClientPacketFactory;
};

/**
 * Reads the source files below the configured roots, sorted by relative path: C++ files (.cpp, .h) under aion/gameserver/handlers and
 * aion/gameserver/network/aion/clientpackets, Java files (.java) under the Java handler directory. A configured root that does not exist, an
 * unreadable file or a C++ source with another extension (.hpp, .cc, .cxx, .inl, .ipp, .hh) is an error.
 */
ScanInput readInput(const Options& options, std::vector<Diagnostic>& errors);

/** One registry row, ready for the emitter. */
struct RegistryEntry {
	MarkerKind kind;
	std::string key;        // AI name, zone names, client packet name; empty for number keys and commands
	int32_t number = 0;     // map id, quest id, zone quest id
	std::string javaClass;  // "ai.AggressiveNpcAI", "CM_MOVE"
	std::string ns;         // namespace of the factory
	std::string function;   // factory function name, e.g. "AggressiveNpcAI_aiFactory"
	std::string source;     // "ai/AggressiveNpcAI.cpp:88"
};

struct ReportSection {
	std::string title;
	size_t ported = 0;
	std::optional<size_t> java;                  // nullopt: no Java input
	std::vector<std::string> missing;            // Java keys without a C++ marker: "key<TAB>javaClass"
	std::vector<std::string> unknownToJava;      // C++ keys Java does not know (no Java class at the mirrored path)
};

struct RegistryModel {
	std::vector<RegistryEntry> ai;            // sorted by name
	std::vector<RegistryEntry> instances;     // sorted by map id
	std::vector<RegistryEntry> zones;         // sorted by zone names
	std::vector<RegistryEntry> quests;        // sorted by quest id
	std::vector<RegistryEntry> commands;      // sorted by kind, then Java class
	std::vector<RegistryEntry> clientPackets; // sorted by name
	std::set<int32_t> npcIds;                 // spawned by the C++ ai/instance/quest sources
	std::map<std::string, size_t> filesPerCategory; // handler files per first directory ("ai", ...), client packet files under "clientpackets"
	std::vector<ReportSection> report;
	std::vector<Diagnostic> errors; // sorted, unique
};

RegistryModel buildRegistry(const ScanInput& input);

/** "aion/gameserver/handlers/quest/template_/X.cpp" + "X" -> "quest.template.X"; client packets: the class name */
std::string javaClassName(const Marker& marker);

} // namespace aion::gameserver::tools::regscan
