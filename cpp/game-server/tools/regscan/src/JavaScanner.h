#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "Lexer.h"

/**
 * Reads the registration keys of the Java handlers (game-server/data/handlers) and the client packet table of AionClientPacketFactory.java, for
 * the registry report and the Java cross-checks. It follows the Java class listeners (AIHandlerClassListener, InstanceHandlerClassListener,
 * ZoneHandlerClassListener, QuestHandlerLoader, ChatCommandsLoader): every compiled class that is public and not abstract registers, top-level
 * classes and public static nested classes alike. Annotation keys (@AIName, @InstanceID, @ZoneNameAnnotation) are read per file; quest handlers
 * and commands are recognized by `isAssignableFrom`, i.e. through the whole superclass chain, which resolveJavaHandlerRegistrations() resolves
 * over all scanned files after the last scanJavaHandlerFile call.
 */
namespace aion::gameserver::tools::regscan {

enum class CommandKind { ADMIN, PLAYER, CONSOLE };

struct JavaZoneKey {
	std::string names;
	int32_t questId = 0;
};

struct JavaHandlerClass {
	std::string javaClass; // package + "." + simple name, e.g. "ai.instance.darkPoeta.CalindiFlamelordAI"; nested: "pkg.Outer.Inner"
	std::string file;
	uint32_t line = 0;
	uint32_t column = 0;
	bool registered = false;               // registered by a Java class listener (public, non-abstract, with its key); keys are only set then
	std::optional<std::string> aiName;     // @AIName (ai/)
	std::optional<int32_t> instanceId;     // @InstanceID (instance/)
	std::optional<JavaZoneKey> zone;       // @ZoneNameAnnotation (zone/)
	std::optional<int32_t> questId;        // super(id) of an AbstractQuestHandler subclass (quest/)
	std::optional<CommandKind> command;    // AdminCommand / PlayerCommand / ConsoleCommand subclass

	// scanner state for resolveJavaHandlerRegistrations
	std::string extends;                   // the superclass as written (dotted, without type arguments), empty if none
	std::string enclosingClass;            // javaClass of the enclosing class of a nested class, empty for a top-level class
	bool instantiable = false;             // public and not abstract (nested: also static), what the class listeners accept
	size_t source = 0;                     // index into JavaHandlerScan::sources
	std::optional<int32_t> constructorSuperId; // quest/: the int passed to super(...) in its constructor, if it is a literal or int constant
};

/** Package and imports of one scanned Java file (superclass name resolution). */
struct JavaSource {
	std::string category;                                 // first directory below data/handlers ("quest", "admincommands", ...)
	std::string package;
	std::map<std::string, std::string, std::less<>> singleImports; // simple name -> qualified name
	std::vector<std::string> onDemandImports;             // packages or classes of `import x.y.*;`
};

struct JavaHandlerScan {
	std::vector<JavaHandlerClass> classes; // every class, top-level and nested (see registered)
	std::vector<JavaSource> sources;
	std::set<int32_t> npcIds;              // QuestSpawnAnalyzer's ids over ai/, instance/ and quest/
	size_t files = 0;
	std::vector<Diagnostic> errors;
	bool resolved = false;
};

/** @param relPath path relative to data/handlers with forward slashes, e.g. "ai/GeneralNpcAI.java" */
void scanJavaHandlerFile(std::string_view file, std::string_view relPath, std::string_view content, JavaHandlerScan& scan);

/**
 * Registers the quest handlers and commands of a complete scan: resolves each instantiable class's superclass chain through nested classes,
 * single-type imports, the class's package, on-demand imports and the known core classes (AbstractQuestHandler and the core quest templates,
 * ChatCommand, AdminCommand, PlayerCommand, ConsoleCommand), and sets questId/command/registered. A chain that reaches a class of a core handler
 * package that regscan does not know, a cycle, or a direct ChatCommand subclass is an error. Call once, after all files were scanned.
 * @throws std::logic_error if called twice
 */
void resolveJavaHandlerRegistrations(JavaHandlerScan& scan);

struct JavaClientPacket {
	std::string name; // e.g. "CM_MOVE"
	int32_t opcode = 0;
	uint32_t line = 0;
};

/** The active <tt>packets[N] = new PacketInfo<>(CM_X.class, ...)</tt> entries of AionClientPacketFactory.java, in file order. */
std::vector<JavaClientPacket> scanJavaClientPacketFactory(std::string_view file, std::string_view content, std::vector<Diagnostic>& errors);

} // namespace aion::gameserver::tools::regscan
