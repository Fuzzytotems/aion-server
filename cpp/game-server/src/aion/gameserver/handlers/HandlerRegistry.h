#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"

/**
 * Handler registration (replaces the Java script engines' class listeners: AIEngine/AIName, InstanceEngine/InstanceID, ZoneService/
 * ZoneNameAnnotation, QuestHandlerLoader, ChatCommandsLoader and the reflective AionClientPacketFactory table).
 * See docs/design/handlers-and-porting-plan.md §1 and its amendments.
 * <p>
 * Every handler .cpp ends with a one-line marker at namespace scope, directly in the package namespace of its directory:
 * <pre>
 * namespace aion::gameserver::handlers::ai {
 * class AggressiveNpcAI : public GeneralNpcAI { ... };
 * AION_AI(AggressiveNpcAI, "aggressive");                                   // Java: @AIName("aggressive")
 * }
 * AION_INSTANCE_HANDLER(BaranathDredgionInstance, 300110000);                // Java: @InstanceID(300110000)
 * AION_ZONE_HANDLER(_1012SensoryArea, "LF1A_SENSORYAREA_Q1012_2_206005_4_210030000 ...", 1012); // @ZoneNameAnnotation(value, questId)
 * AION_ZONE_HANDLER(PvPAreaZone, "LC1_PVP_SUB_C_110010000 DC1_PVP_ZONE_120010000");         // questId defaults to 0
 * AION_QUEST_HANDLER(_1500OrdersFromPerento, 1500);                          // Java: super(1500) in the constructor
 * AION_ADMIN_COMMAND(Add);  AION_PLAYER_COMMAND(Id);  AION_CONSOLE_COMMAND(Attrbonus);
 * AION_CLIENT_PACKET(CM_MOVE);                                                // in network/aion/clientpackets, namespace ...::clientpackets
 * </pre>
 * Marker rules (checked by the build tool aion_gs_regscan, see game-server/tools/regscan/README.md): the whole marker on one line, literal
 * arguments only (a plain "string" without escapes, a decimal int), in a .cpp file (never a header), outside #if blocks, at the package
 * namespace of the file's directory, and the class defined in that namespace. Keys are unique (AI name, map id, zone name, quest id, class).
 * <p>
 * Each marker expands to a concept-checked factory function with external linkage, <tt>Class_aiFactory</tt> etc., in the handler's
 * namespace. aion_gs_regscan writes constinit tables (Registry.*.gen.cpp) that declare and reference these functions, so:
 * - every handler .obj is pulled out of its static library without /WHOLEARCHIVE, and nothing depends on static initialization order;
 * - a marker in the wrong namespace is an unresolved external at link time;
 * - tables are sorted by key and contain no code: no logging, no config access before main.
 * <p>
 * Engines read the tables only in their init() (GameServer::main after config and static data). Quests and commands are created once and
 * registered as Immortal singletons; AIs (a Creature part), instance handlers and zone handlers are created per owner.
 * Tests that need no handlers link the *_empty registry libraries, which define the same accessors with empty tables.
 */

// Core types used by the factory signatures. The S0b hub headers define them; they must stay non-template classes in these namespaces.
namespace aion::gameserver::ai {
class AbstractAI;
}
namespace aion::gameserver::model::gameobjects {
class Creature;
}
namespace aion::gameserver::instance::handlers {
class InstanceHandler;
}
namespace aion::gameserver::world {
class WorldMapInstance;
}
namespace aion::gameserver::world::zone::handler {
class ZoneHandler;
class QuestZoneHandler;
} // namespace aion::gameserver::world::zone::handler
namespace aion::gameserver::questEngine::handlers {
class AbstractQuestHandler;
}
namespace aion::gameserver::utils::chathandlers {
class ChatCommand;
class AdminCommand;
class PlayerCommand;
class ConsoleCommand;
} // namespace aion::gameserver::utils::chathandlers
namespace aion::gameserver::network::aion {
class AionClientPacket;
/** Java: Set<AionConnection.State> (an EnumSet) of the states in which a client packet is accepted. Defined by P4-15. */
class StateSet;
} // namespace aion::gameserver::network::aion

namespace aion::gameserver::handlers {

// Names inside this namespace are always fully qualified: the handler packages ai, instance, ... shadow the core namespaces of the same name.

/**
 * Java: AIEngine.newAI's reflective construction. Creates the AI for the given owner, or returns nullptr if the owner's dynamic type is not the
 * AI's OwnerType (AIEngine throws IllegalArgumentException(aiOwnerMismatchMessage(...)) then, like Java's findConstructor miss).
 */
using AIFactory = std::unique_ptr<::aion::gameserver::ai::AbstractAI>(::aion::gameserver::model::gameobjects::Creature& owner);
/** Java: getDeclaredConstructor(WorldMapInstance.class).newInstance(instance). Exceptions of the constructor propagate. */
using InstanceFactory = ::aion::gameserver::runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
	::aion::gameserver::world::WorldMapInstance& instance);
/** Java: getDeclaredConstructor().newInstance(); a QuestZoneHandler receives the questId of its marker (Java reads its own annotation). */
using ZoneFactory = ::aion::gameserver::runtime::Ref<::aion::gameserver::world::zone::handler::ZoneHandler>(int32_t questId);
/** Java: QuestHandlerLoader's getDeclaredConstructor().newInstance() */
using QuestFactory = std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler>();
/** Java: ChatCommandsLoader's getDeclaredConstructor().newInstance() */
using CommandFactory = std::unique_ptr<::aion::gameserver::utils::chathandlers::ChatCommand>();
/** Java: PacketInfo.newPacket's packetConstructor.newInstance(opCode, validStates) */
using ClientPacketFactory = std::unique_ptr<::aion::gameserver::network::aion::AionClientPacket>(
	int32_t opcode, const ::aion::gameserver::network::aion::StateSet& validStates);

/** One AION_AI marker. Table sorted by name (byte order). */
struct AIHandlerEntry {
	std::string_view name;      // Java: @AIName value
	std::string_view javaClass; // Java class name relative to data/handlers, e.g. "ai.instance.darkPoeta.CalindiFlamelordAI"
	AIFactory* create;
	std::string_view source; // "ai/instance/darkPoeta/CalindiFlamelordAI.cpp:88" (relative to aion/gameserver/handlers)
};

/** One AION_INSTANCE_HANDLER marker. Table sorted by mapId. */
struct InstanceHandlerEntry {
	int32_t mapId; // Java: @InstanceID value
	std::string_view javaClass;
	InstanceFactory* create;
	std::string_view source;
};

/** One AION_ZONE_HANDLER marker. Table sorted by zoneNames (byte order); every name occurs in one entry only. */
struct ZoneHandlerEntry {
	std::string_view zoneNames; // Java: @ZoneNameAnnotation value, names separated by single spaces (see zoneNamesOf)
	int32_t questId;            // Java: @ZoneNameAnnotation questId (default 0)
	std::string_view javaClass;
	ZoneFactory* create;
	std::string_view source;
};

/** One AION_QUEST_HANDLER marker. Table sorted by questId. QuestEngine checks create()->getQuestId() == questId (fatal on mismatch). */
struct QuestHandlerEntry {
	int32_t questId;
	std::string_view javaClass;
	QuestFactory* create;
	std::string_view source;
};

/** The command base class of a marker, i.e. the handler directory (Java: CommandsConfig.HANDLER_DIRECTORIES). */
enum class CommandKind { ADMIN, PLAYER, CONSOLE };

/** One AION_ADMIN_COMMAND / AION_PLAYER_COMMAND / AION_CONSOLE_COMMAND marker. Table sorted by kind, then javaClass. */
struct CommandEntry {
	CommandKind kind;
	std::string_view javaClass; // e.g. "admincommands.Add"
	CommandFactory* create;
	std::string_view source;
};

/** One AION_CLIENT_PACKET marker. Table sorted by name. The opcode table (ClientPacketInfo.gen.inc) maps opcodes to these names. */
struct ClientPacketEntry {
	std::string_view name; // simple class name, e.g. "CM_MOVE"
	ClientPacketFactory* create;
	std::string_view source; // "network/aion/clientpackets/CM_MOVE.cpp:40" (relative to aion/gameserver)
};

// Defined in the generated registry TUs (Registry.<name>.gen.cpp), or with empty tables in the *_empty variants.
std::span<const AIHandlerEntry> aiHandlerEntries() noexcept;
std::span<const InstanceHandlerEntry> instanceHandlerEntries() noexcept;
std::span<const ZoneHandlerEntry> zoneHandlerEntries() noexcept;
std::span<const QuestHandlerEntry> questHandlerEntries() noexcept;
std::span<const CommandEntry> commandEntries() noexcept;
std::span<const ClientPacketEntry> clientPacketEntries() noexcept;
/**
 * QuestSpawnAnalyzer.loadNpcIdsSpawnedByHandlers replacement: the npc ids that the compiled-in handler sources of ai, instance and quest spawn,
 * found at build time with Java's pattern \bsp(?:awn)?\([^,\d]*(\d{6})(?: : (\d{6}))? over the raw file text. Sorted, unique.
 */
std::span<const int32_t> npcIdsSpawnedByHandlers() noexcept;

/** @return the entry with the given AI name, nullptr if none (binary search over the sorted table) */
inline const AIHandlerEntry* findAIHandler(std::span<const AIHandlerEntry> entries, std::string_view name) noexcept {
	auto it = std::ranges::lower_bound(entries, name, {}, &AIHandlerEntry::name);
	return it != entries.end() && it->name == name ? &*it : nullptr;
}

/** @return the entry for the given map id, nullptr if none */
inline const InstanceHandlerEntry* findInstanceHandler(std::span<const InstanceHandlerEntry> entries, int32_t mapId) noexcept {
	auto it = std::ranges::lower_bound(entries, mapId, {}, &InstanceHandlerEntry::mapId);
	return it != entries.end() && it->mapId == mapId ? &*it : nullptr;
}

/** @return the entry for the given quest id, nullptr if none */
inline const QuestHandlerEntry* findQuestHandler(std::span<const QuestHandlerEntry> entries, int32_t questId) noexcept {
	auto it = std::ranges::lower_bound(entries, questId, {}, &QuestHandlerEntry::questId);
	return it != entries.end() && it->questId == questId ? &*it : nullptr;
}

/** @return the entry of the client packet class with the given simple name, nullptr if none */
inline const ClientPacketEntry* findClientPacket(std::span<const ClientPacketEntry> entries, std::string_view name) noexcept {
	auto it = std::ranges::lower_bound(entries, name, {}, &ClientPacketEntry::name);
	return it != entries.end() && it->name == name ? &*it : nullptr;
}

/** Java: idAnnotation.value().split(" ") - the zone names of an entry (the build tool guarantees single spaces and no empty names). */
inline std::vector<std::string_view> zoneNamesOf(const ZoneHandlerEntry& entry) {
	std::vector<std::string_view> names;
	std::string_view rest = entry.zoneNames;
	while (!rest.empty()) {
		size_t space = rest.find(' ');
		names.push_back(rest.substr(0, space));
		rest = space == std::string_view::npos ? std::string_view() : rest.substr(space + 1);
	}
	return names;
}

/** Java: AIEngine.newAI's message "class ai.X cannot be instantiated with Summon as the owner" (ownerSimpleName: Class.getSimpleName()). */
inline std::string aiOwnerMismatchMessage(const AIHandlerEntry& entry, std::string_view ownerSimpleName) {
	std::string message = "class ";
	message.append(entry.javaClass).append(" cannot be instantiated with ").append(ownerSimpleName).append(" as the owner");
	return message;
}

namespace detail {

/**
 * Java: AIEngine.validateScripts (generic owner type and a constructor taking it), now at compile time. The AbstractAI&lt;T&gt;/AITemplate&lt;T&gt;
 * base exposes the owner type as OwnerType.
 */
template <class C>
concept AIHandlerClass = std::derived_from<C, ::aion::gameserver::ai::AbstractAI> && !std::is_abstract_v<C> && requires { typename C::OwnerType; } &&
	std::derived_from<typename C::OwnerType, ::aion::gameserver::model::gameobjects::Creature> && std::constructible_from<C, typename C::OwnerType&>;

/** RefCounted instance handlers are created with their own static create(WorldMapInstance&), returning Ref to exactly this class. */
template <class C>
concept InstanceHandlerClass = std::derived_from<C, ::aion::gameserver::instance::handlers::InstanceHandler> && !std::is_abstract_v<C> &&
	requires(::aion::gameserver::world::WorldMapInstance& instance) {
		{ C::create(instance) } -> std::same_as<::aion::gameserver::runtime::Ref<C>>;
	};

/** Zone handlers derived from QuestZoneHandler take the marker's questId: static create(int32_t questId). */
template <class C>
concept QuestZoneHandlerClass = std::derived_from<C, ::aion::gameserver::world::zone::handler::QuestZoneHandler> && !std::is_abstract_v<C> &&
	requires(int32_t questId) {
		{ C::create(questId) } -> std::same_as<::aion::gameserver::runtime::Ref<C>>;
	};

/** Other zone handlers: static create() */
template <class C>
concept ZoneHandlerClass = QuestZoneHandlerClass<C> || (std::derived_from<C, ::aion::gameserver::world::zone::handler::ZoneHandler> && !std::is_abstract_v<C> &&
	!std::derived_from<C, ::aion::gameserver::world::zone::handler::QuestZoneHandler> && requires {
		{ C::create() } -> std::same_as<::aion::gameserver::runtime::Ref<C>>;
	});

template <class C>
concept QuestHandlerClass =
	std::derived_from<C, ::aion::gameserver::questEngine::handlers::AbstractQuestHandler> && !std::is_abstract_v<C> && std::default_initializable<C>;

template <class C, class Base>
concept CommandClass = std::derived_from<C, Base> && std::derived_from<C, ::aion::gameserver::utils::chathandlers::ChatCommand> &&
	!std::is_abstract_v<C> && std::default_initializable<C>;

template <class C>
concept ClientPacketClass = std::derived_from<C, ::aion::gameserver::network::aion::AionClientPacket> && !std::is_abstract_v<C> &&
	std::constructible_from<C, int32_t, const ::aion::gameserver::network::aion::StateSet&>;

template <AIHandlerClass C>
std::unique_ptr<::aion::gameserver::ai::AbstractAI> createAI(::aion::gameserver::model::gameobjects::Creature& owner) {
	auto* typedOwner = dynamic_cast<typename C::OwnerType*>(&owner); // Java: findConstructor(aiClass, owner.getClass())
	if (typedOwner == nullptr)
		return nullptr;
	return std::make_unique<C>(*typedOwner);
}

template <ZoneHandlerClass C>
::aion::gameserver::runtime::Ref<::aion::gameserver::world::zone::handler::ZoneHandler> createZoneHandler(int32_t questId) {
	if constexpr (QuestZoneHandlerClass<C>)
		return C::create(questId);
	else
		return C::create();
}

/** The optional questId argument of AION_ZONE_HANDLER */
constexpr int32_t zoneQuestId() noexcept {
	return 0;
}
constexpr int32_t zoneQuestId(int32_t questId) noexcept {
	return questId;
}

/** Marker keys are plain int literals (the build tool checks the spelling) */
template <class T>
constexpr bool isPositiveIntKey(T value) noexcept {
	return std::is_same_v<T, int> && value > 0;
}

} // namespace detail

} // namespace aion::gameserver::handlers

// clang-format off

/** Java: @AIName(name). One line, at the package namespace, in the handler's .cpp. */
#define AION_AI(Class, name)                                                                                                                         \
	static_assert(::aion::gameserver::handlers::detail::AIHandlerClass<Class>,                                                                          \
		"AION_AI: the class must be a non-abstract AbstractAI whose OwnerType derives from Creature, constructible from OwnerType&");                   \
	::std::unique_ptr<::aion::gameserver::ai::AbstractAI> Class##_aiFactory(::aion::gameserver::model::gameobjects::Creature& owner) {                  \
		return ::aion::gameserver::handlers::detail::createAI<Class>(owner);                                                                             \
	}                                                                                                                                                    \
	static_assert(sizeof("" name) > 1, "AION_AI: the name must be a non-empty string literal")

/** Java: @InstanceID(mapId) */
#define AION_INSTANCE_HANDLER(Class, mapId)                                                                                                          \
	static_assert(::aion::gameserver::handlers::detail::InstanceHandlerClass<Class>,                                                                    \
		"AION_INSTANCE_HANDLER: the class must be a non-abstract InstanceHandler with static Ref<Class> create(WorldMapInstance&)");                    \
	::aion::gameserver::runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler> Class##_instanceFactory(                                \
		::aion::gameserver::world::WorldMapInstance& instance) {                                                                                         \
		return Class::create(instance);                                                                                                                  \
	}                                                                                                                                                    \
	static_assert(::aion::gameserver::handlers::detail::isPositiveIntKey(mapId), "AION_INSTANCE_HANDLER: the map id must be a positive int literal")

/** Java: @ZoneNameAnnotation(value = zoneNames[, questId = N]) */
#define AION_ZONE_HANDLER(Class, zoneNames, ...)                                                                                                     \
	static_assert(::aion::gameserver::handlers::detail::ZoneHandlerClass<Class>,                                                                        \
		"AION_ZONE_HANDLER: the class must be a non-abstract ZoneHandler with static Ref<Class> create() (QuestZoneHandler: create(int32_t questId))"); \
	static_assert(!::aion::gameserver::handlers::detail::QuestZoneHandlerClass<Class> ||                                                                \
		::aion::gameserver::handlers::detail::zoneQuestId(__VA_ARGS__) > 0, "AION_ZONE_HANDLER: a QuestZoneHandler needs a questId > 0");               \
	::aion::gameserver::runtime::Ref<::aion::gameserver::world::zone::handler::ZoneHandler> Class##_zoneFactory(int32_t questId) {                     \
		return ::aion::gameserver::handlers::detail::createZoneHandler<Class>(questId);                                                                  \
	}                                                                                                                                                    \
	static_assert(sizeof("" zoneNames) > 1, "AION_ZONE_HANDLER: the zone names must be a non-empty string literal")

/** Java: an AbstractQuestHandler subclass calling super(questId) */
#define AION_QUEST_HANDLER(Class, questId)                                                                                                           \
	static_assert(::aion::gameserver::handlers::detail::QuestHandlerClass<Class>,                                                                       \
		"AION_QUEST_HANDLER: the class must be a non-abstract, default constructible AbstractQuestHandler");                                             \
	::std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> Class##_questFactory() {                                         \
		return ::std::make_unique<Class>();                                                                                                              \
	}                                                                                                                                                    \
	static_assert(::aion::gameserver::handlers::detail::isPositiveIntKey(questId), "AION_QUEST_HANDLER: the quest id must be a positive int literal")

#define AION_DETAIL_COMMAND(Class, Base)                                                                                                             \
	static_assert(::aion::gameserver::handlers::detail::CommandClass<Class, Base>,                                                                      \
		"AION_*_COMMAND: the class must be a non-abstract, default constructible command of the marker's kind");                                         \
	::std::unique_ptr<::aion::gameserver::utils::chathandlers::ChatCommand> Class##_commandFactory() {                                                  \
		return ::std::make_unique<Class>();                                                                                                              \
	}                                                                                                                                                    \
	static_assert(sizeof(Class) > 0, "AION_*_COMMAND: the class must be complete")

/** Java: a public AdminCommand subclass in data/handlers/admincommands */
#define AION_ADMIN_COMMAND(Class) AION_DETAIL_COMMAND(Class, ::aion::gameserver::utils::chathandlers::AdminCommand)
/** Java: a public PlayerCommand subclass in data/handlers/playercommands */
#define AION_PLAYER_COMMAND(Class) AION_DETAIL_COMMAND(Class, ::aion::gameserver::utils::chathandlers::PlayerCommand)
/** Java: a public ConsoleCommand subclass in data/handlers/consolecommands */
#define AION_CONSOLE_COMMAND(Class) AION_DETAIL_COMMAND(Class, ::aion::gameserver::utils::chathandlers::ConsoleCommand)

/** Java: new PacketInfo<>(CM_X.class, states...) in AionClientPacketFactory */
#define AION_CLIENT_PACKET(Class)                                                                                                                    \
	static_assert(::aion::gameserver::handlers::detail::ClientPacketClass<Class>,                                                                       \
		"AION_CLIENT_PACKET: the class must be a non-abstract AionClientPacket constructible from (int32_t opcode, const StateSet& validStates)");       \
	::std::unique_ptr<::aion::gameserver::network::aion::AionClientPacket> Class##_clientPacketFactory(                                               \
		int32_t opcode, const ::aion::gameserver::network::aion::StateSet& validStates) {                                                               \
		return ::std::make_unique<Class>(opcode, validStates);                                                                                           \
	}                                                                                                                                                    \
	static_assert(sizeof(Class) > 0, "AION_CLIENT_PACKET: the class must be complete")

// clang-format on
