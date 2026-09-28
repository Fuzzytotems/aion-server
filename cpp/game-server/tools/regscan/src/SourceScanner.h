#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Lexer.h"

/** Per-file scan of C++ handler and client packet sources: markers, namespace blocks, type definitions and the unity file rules. */
namespace aion::gameserver::tools::regscan {

enum class MarkerKind { AI, INSTANCE_HANDLER, ZONE_HANDLER, QUEST_HANDLER, ADMIN_COMMAND, PLAYER_COMMAND, CONSOLE_COMMAND, CLIENT_PACKET };

/** @return the macro name, e.g. "AION_AI" */
std::string_view markerName(MarkerKind kind) noexcept;

/** @return the handler directory of the marker kind ("ai", "admincommands", ...), empty for CLIENT_PACKET */
std::string_view markerCategory(MarkerKind kind) noexcept;

struct Marker {
	MarkerKind kind;
	std::string className;
	std::string text;               // AI name or zone names
	std::optional<int32_t> number;  // map id, quest id, or the zone handler's quest id
	std::string ns;                 // C++ namespace, "a::b::c"
	std::string file;               // display path
	std::string relPath;            // path relative to the include root, forward slashes
	uint32_t line = 0;
	uint32_t column = 0;
};

struct TypeDefinition {
	std::string ns;
	std::string name;
	std::string file;
	uint32_t line = 0;
	uint32_t column = 0;
};

/** HANDLER files live below aion/gameserver/handlers and follow the unity file rules; CLIENT_PACKET files are core sources. */
enum class FileRole { HANDLER, CLIENT_PACKET };

struct FileScan {
	std::vector<Marker> markers;
	std::vector<TypeDefinition> types;
	std::vector<Diagnostic> errors;
};

/**
 * @param file display path used in diagnostics
 * @param relPath path relative to the include root with forward slashes, e.g. "aion/gameserver/handlers/ai/GeneralNpcAI.cpp"; its directory
 * gives the expected namespace
 */
FileScan scanCppFile(std::string_view file, std::string_view relPath, std::string_view content, FileRole role);

/**
 * The namespace a file's directory requires: "aion/gameserver/handlers/quest/template/X.cpp" -> "aion::gameserver::handlers::quest::template_"
 * (Java identifiers that are C++ keywords get a trailing underscore; a directory may also be spelled with the underscore).
 */
std::string expectedNamespace(std::string_view relPath);

} // namespace aion::gameserver::tools::regscan
