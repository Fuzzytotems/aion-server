// Spine prototype against the real hub headers (handlers-and-porting-plan.md "Risks": create<T>/postConstruct plus one effect and one AI before
// the freeze). Test doubles stand in for data (default-constructed static templates, SpawnGroup/SpawnTemplate) and for services that are not
// ported yet (AIEngine::newAI: the AI is installed like `//ai set`).
//
// What is checked: VisibleObject::create<T> with the postConstruct chain (virtual createAggroList dispatch, late-bound controller owner), part
// ownership (OwnedPart bound to the creature, Ref<Part> and Pin retain the owner), Ref/Ptr use (cast, Ptr from parts, Effect holding Refs),
// an AI created through the HandlerRegistry AI factory shape, an Effect on a creature, a part pinned by a scheduled task (and an AI replaced
// while that task is pending), destruction through the Reclaimer and an empty LeakCensus. One mock handler of each registry kind is compiled
// against the real bases and HandlerRegistry.h markers; the command factories create their commands (ChatCommand's ported constructor).
//
// Guards: the scenario needs definitions that the hub .cpp files keep behind S0b transition / member-type guards until non-hub headers exist
// (TransformModel, AIEventLog, AttackCalcObserver, TerrainZoneCollisionMaterialActor, AggroInfo, EffectReserved, KnownObject; for Npc also
// NpcMoveController, NpcSkillList, NpcSkillEntry, NpcGameStats, NpcLifeStats, WalkerGroup). Without them only the compile-time checks and the
// handler shapes that already link are built. A scenario that reaches AION_UNPORTED is skipped with the name of the blocking body.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "aion/gameserver/ai/AITemplate.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/chathandlers/AdminCommand.h"
#include "aion/gameserver/utils/chathandlers/ConsoleCommand.h"
#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/zone/handler/GeneralZoneHandler.h"
#include "aion/gameserver/world/zone/handler/QuestZoneHandler.h"

// Definitions of Creature, its parts and Effect that exist only with these (non-hub) headers.
#if __has_include("aion/gameserver/model/gameobjects/TransformModel.h") && __has_include("aion/gameserver/ai/event/AIEventLog.h") && \
	__has_include("aion/gameserver/controllers/observer/AttackCalcObserver.h") && \
	__has_include("aion/gameserver/controllers/observer/TerrainZoneCollisionMaterialActor.h") && \
	__has_include("aion/gameserver/controllers/attack/AggroInfo.h") && __has_include("aion/gameserver/skillengine/model/EffectReserved.h") && \
	__has_include("aion/gameserver/world/knownlist/KnownObject.h")
#define AION_PROTOTYPE_CREATURE 1
#else
#define AION_PROTOTYPE_CREATURE 0
#endif
// Npc's constructor, destructor and postConstruct (Npc.cpp member-type guard).
#if AION_PROTOTYPE_CREATURE && __has_include("aion/gameserver/controllers/movement/NpcMoveController.h") && \
	__has_include("aion/gameserver/model/skill/NpcSkillList.h") && __has_include("aion/gameserver/model/skill/NpcSkillEntry.h") && \
	__has_include("aion/gameserver/model/stats/container/NpcGameStats.h") && __has_include("aion/gameserver/model/stats/container/NpcLifeStats.h") && \
	__has_include("aion/gameserver/spawnengine/WalkerGroup.h")
#define AION_PROTOTYPE_NPC 1
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#else
#define AION_PROTOTYPE_NPC 0
#endif
// QuestEngine::getInstance (AbstractQuestHandler's constructor) and QuestZoneHandler's constructor.
#if __has_include("aion/gameserver/model/templates/quest/QuestNpc.h")
#define AION_PROTOTYPE_QUEST_HANDLER 1
#else
#define AION_PROTOTYPE_QUEST_HANDLER 0
#endif
#if __has_include("aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h")
#define AION_PROTOTYPE_QUEST_ZONE_HANDLER 1
#else
#define AION_PROTOTYPE_QUEST_ZONE_HANDLER 0
#endif

namespace aion::gameserver::prototype {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using runtime::Ptr;
using runtime::Ref;

// ---------------------------------------------------------------------------------------------------------------------------------------
// Compile-time shape checks (always built)
// ---------------------------------------------------------------------------------------------------------------------------------------

// Construction goes only through VisibleObject::create<T>: no public constructor without the CreateKey passkey.
static_assert(!std::is_constructible_v<Npc, std::unique_ptr<controllers::NpcController>, model::templates::spawns::SpawnTemplate&,
	const model::templates::npc::NpcTemplate*>);
// Late-bound controllers and AI are parts (OwnedPart): Ref<part> / Pin(part) retain the owning creature.
static_assert(std::derived_from<controllers::VisibleObjectController, runtime::OwnedPart>);
static_assert(std::derived_from<ai::AbstractAI, runtime::OwnedPart> && std::derived_from<controllers::attack::AggroList, runtime::OwnedPart>);
static_assert(std::derived_from<controllers::effect::EffectController, runtime::OwnedPart>);
static_assert(std::derived_from<world::knownlist::KnownList, runtime::OwnedPart>);
static_assert(runtime::Pinnable<ai::NpcAI> && runtime::Pinnable<Npc> && runtime::Pinnable<skillengine::model::Effect>);
// Effect is RefCounted and Ref-held as StatOwner (retain/release forwarded).
static_assert(runtime::Retainable<skillengine::model::Effect>);
// Part accessors return references, Java-nullable parts return Ptr (hub-headers.md §5, Creature.h class comment).
static_assert(std::same_as<decltype(std::declval<const Npc&>().getController()), controllers::NpcController&>);
static_assert(std::same_as<decltype(std::declval<const Creature&>().getAi()), ai::AbstractAI&>);
static_assert(std::same_as<decltype(std::declval<const Creature&>().getAggroList()), controllers::attack::AggroList&>);
static_assert(std::same_as<decltype(std::declval<const Creature&>().getEffectController()), Ptr<controllers::effect::EffectController>>);
static_assert(std::same_as<decltype(std::declval<const controllers::NpcController&>().getOwner()), Npc&>);
static_assert(std::same_as<decltype(std::declval<const ai::NpcAI&>().getOwner()), Npc&>);
static_assert(std::same_as<ai::NpcAI::OwnerType, Npc>);

// ---------------------------------------------------------------------------------------------------------------------------------------
// Mock handlers of each registry kind, written as handler files will be (marker after the class, in the handler's package namespace)
// ---------------------------------------------------------------------------------------------------------------------------------------

} // namespace aion::gameserver::prototype

namespace aion::gameserver::handlers::ai::prototype {

/** AI handler: NpcAI subclass, created by the AI registry with std::make_unique<C>(npc). */
class PrototypeNpcAI final : public gameserver::ai::NpcAI {
public:
	static inline std::atomic<int32_t> live{0};

	explicit PrototypeNpcAI(model::gameobjects::Npc& owner) : NpcAI(owner) { live.fetch_add(1); }
	~PrototypeNpcAI() override { live.fetch_sub(1); }
};
static_assert(handlers::detail::AIHandlerClass<PrototypeNpcAI>);
#if AION_PROTOTYPE_CREATURE
AION_AI(PrototypeNpcAI, "prototype_npc");
#endif

} // namespace aion::gameserver::handlers::ai::prototype

namespace aion::gameserver::handlers::instance::prototype {

class PrototypeInstance final : public gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<PrototypeInstance> create(world::WorldMapInstance& instance) { return runtime::makeRef<PrototypeInstance>(instance); }

	void onInstanceCreate() override {}

protected:
	explicit PrototypeInstance(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}
	~PrototypeInstance() override = default;
};
AION_INSTANCE_HANDLER(PrototypeInstance, 300110000);

} // namespace aion::gameserver::handlers::instance::prototype

namespace aion::gameserver::handlers::zone::prototype {

class PrototypeQuestArea final : public world::zone::handler::QuestZoneHandler {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<PrototypeQuestArea> create(int32_t questId) { return runtime::makeRef<PrototypeQuestArea>(questId); }

	runtime::Ref<controllers::observer::AbstractQuestZoneObserver> createObserver(model::gameobjects::player::Player& player,
		const model::templates::zone::ZoneTemplate* zoneTemplate) override {
		AION_UNPORTED();
	}

protected:
	explicit PrototypeQuestArea(int32_t questId) : QuestZoneHandler(questId) {}
	~PrototypeQuestArea() override = default;
};
static_assert(handlers::detail::ZoneHandlerClass<PrototypeQuestArea> && handlers::detail::QuestZoneHandlerClass<PrototypeQuestArea>);
#if AION_PROTOTYPE_QUEST_ZONE_HANDLER
AION_ZONE_HANDLER(PrototypeQuestArea, "PROTOTYPE_QUEST_AREA_210010000", 1012);
#endif

class PrototypePvPZone final : public world::zone::handler::GeneralZoneHandler {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<PrototypePvPZone> create() { return runtime::makeRef<PrototypePvPZone>(); }

	void onEnterZone(model::gameobjects::Creature& player, world::zone::ZoneInstance& zone) override {}

protected:
	PrototypePvPZone() = default;
	~PrototypePvPZone() override = default;
};
AION_ZONE_HANDLER(PrototypePvPZone, "PROTOTYPE_PVP_ZONE_210010000");

} // namespace aion::gameserver::handlers::zone::prototype

namespace aion::gameserver::handlers::quest::prototype {

class _1500PrototypeQuest final : public questEngine::handlers::AbstractQuestHandler {
public:
	_1500PrototypeQuest() : AbstractQuestHandler(1500) {}

	void register_() override {}
};
static_assert(handlers::detail::QuestHandlerClass<_1500PrototypeQuest>);
#if AION_PROTOTYPE_QUEST_HANDLER
AION_QUEST_HANDLER(_1500PrototypeQuest, 1500);
#endif

} // namespace aion::gameserver::handlers::quest::prototype

namespace aion::gameserver::handlers::admincommands::prototype {

class PrototypeAdmin final : public utils::chathandlers::AdminCommand {
public:
	PrototypeAdmin() : AdminCommand("prototype", "Prototype command.", "<id> - prints the id") {}

protected:
	void execute(model::gameobjects::player::Player& player, std::span<const std::string> params) override {
		if (params.empty())
			sendInfo(player);
		else
			sendInfo(player, "id: ", params[0]);
	}
};
AION_ADMIN_COMMAND(PrototypeAdmin);

} // namespace aion::gameserver::handlers::admincommands::prototype

namespace aion::gameserver::handlers::playercommands::prototype {

class PrototypePlayerCommand final : public utils::chathandlers::PlayerCommand {
public:
	PrototypePlayerCommand() : PlayerCommand("prototype", "Prototype command.") {}

protected:
	void execute(model::gameobjects::player::Player& player, std::span<const std::string> params) override { sendInfo(player); }
};
AION_PLAYER_COMMAND(PrototypePlayerCommand);

} // namespace aion::gameserver::handlers::playercommands::prototype

namespace aion::gameserver::handlers::consolecommands::prototype {

class PrototypeConsoleCommand final : public utils::chathandlers::ConsoleCommand {
public:
	PrototypeConsoleCommand() : ConsoleCommand("prototype") {}

protected:
	void execute(model::gameobjects::player::Player& admin, std::span<const std::string> params) override { sendInfo(admin); }
};
AION_CONSOLE_COMMAND(PrototypeConsoleCommand);

} // namespace aion::gameserver::handlers::consolecommands::prototype

namespace aion::gameserver::prototype {
namespace {

TEST(SpinePrototypeHandlers, GeneralZoneHandlerIsHeldByRefThroughTheZoneHandlerInterface) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<world::zone::handler::ZoneHandler> handler = handlers::zone::prototype::PrototypePvPZone_zoneFactory(0);
		ASSERT_TRUE(handler);
		auto* zone = dynamic_cast<handlers::zone::prototype::PrototypePvPZone*>(handler.get());
		ASSERT_NE(zone, nullptr);
		EXPECT_EQ(zone->refCount(), 1u); // Ref<ZoneHandler> retains through the forwarded retain()
		Ref<world::zone::handler::ZoneHandler> copy = handler;
		EXPECT_EQ(zone->refCount(), 2u);
	}
	runtime::Reclaimer::getInstance().drain();
}

/** A command with a multi-line syntax text (Java text block) */
class SyntaxBlockCommand final : public utils::chathandlers::AdminCommand {
public:
	SyntaxBlockCommand() : AdminCommand("block", "", "- no parameter\n<name> [count] - two parameters\n[f] - flag\nSome help text.\n\n") {}

protected:
	void execute(model::gameobjects::player::Player& player, std::span<const std::string> params) override {}
};

TEST(SpinePrototypeHandlers, CommandFactoriesCreateCommandsWithTheJavaSyntaxInfo) {
	// the registry factories construct commands: ChatCommand's constructor formats the syntax info like Java parseSyntaxInfo
	std::unique_ptr<utils::chathandlers::ChatCommand> admin = handlers::admincommands::prototype::PrototypeAdmin_commandFactory();
	ASSERT_TRUE(admin);
	EXPECT_EQ(admin->getAliasWithPrefix(), "//prototype");
	EXPECT_EQ(admin->getDescription(), "Prototype command.");
	EXPECT_EQ(admin->getSyntaxInfo(), "Syntax:\n\t[color://prototype;1 1 1] <[color:id;1 1 1]> - prints the id");
	std::unique_ptr<utils::chathandlers::ChatCommand> player = handlers::playercommands::prototype::PrototypePlayerCommand_commandFactory();
	EXPECT_EQ(player->getAliasWithPrefix(), ".prototype");
	EXPECT_EQ(player->getSyntaxInfo(), "Syntax:\n\tNo syntax info available.");
	EXPECT_EQ(handlers::consolecommands::prototype::PrototypeConsoleCommand_commandFactory()->getSyntaxInfo(),
		"Syntax:\n\tNo syntax info available.");

	SyntaxBlockCommand block;
	EXPECT_EQ(block.getSyntaxInfo(), "Syntax:\n- no parameter"
		"\n\t[color://block;1 1 1] <[color:name;1 1 1]> [[color:count;1 1 1]] - two parameters"
		"\n\t[color://block;1 1 1] [[color:f\xE2\x80\x8B;1 1 1]] - flag"
		"\nSome help text."
		"\nNote: Parameters enclosed in square brackets are optional.");
}

#if AION_PROTOTYPE_CREATURE

// ---------------------------------------------------------------------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------------------------------------------------------------------

/** Counts destroyed parts to prove the owner's destructor frees them and that virtual createAggroList() dispatched to the subclass. */
struct Destroyed {
	static inline std::atomic<int32_t> npcs{0};
	static inline std::atomic<int32_t> aggroLists{0};
	static inline std::atomic<int32_t> knownLists{0};

	static void reset() {
		npcs = 0;
		aggroLists = 0;
		knownLists = 0;
	}
};

class PrototypeAggroList final : public controllers::attack::AggroList {
public:
	explicit PrototypeAggroList(Creature& owner) : AggroList(owner) {}
	~PrototypeAggroList() override { Destroyed::aggroLists.fetch_add(1); }
};

class PrototypeKnownList final : public world::knownlist::KnownList {
public:
	explicit PrototypeKnownList(VisibleObject& owner) : KnownList(owner) {}
	~PrototypeKnownList() override { Destroyed::knownLists.fetch_add(1); }
};

#if AION_PROTOTYPE_NPC
/**
 * An Npc whose AI is installed like `//ai set` (AIEngine::newAI is not ported): Npc::postConstruct runs the real chain (Creature: AI and aggro
 * list; Npc: controller owner, move controller, stat containers), then the registry's AI factory creates the handler AI.
 */
class PrototypeNpc final : public Npc {
	AION_MAKE_REF_FRIEND
public:
	PrototypeNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, model::templates::spawns::SpawnTemplate& spawnTemplate,
		const model::templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

	int8_t getLevel() override { return 10; }

protected:
	~PrototypeNpc() override { Destroyed::npcs.fetch_add(1); }

	std::unique_ptr<controllers::attack::AggroList> createAggroList() override { return std::make_unique<PrototypeAggroList>(*this); }

	void postConstruct() override {
		Npc::postConstruct();
		replaceAi(handlers::ai::prototype::PrototypeNpcAI_aiFactory(*this)); // Java: AIEngine.newAI("prototype_npc", this)
		setKnownlist(std::make_unique<PrototypeKnownList>(*this));          // Java: SpawnEngine sets the known list before spawning
	}
};
#endif

/** A spawn template part of its group (SpawnTemplate is an OwnedPart of SpawnGroup) */
class TestSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	explicit TestSpawnTemplate(model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 1.5f, 2.5f, 3.5f, int8_t{30}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** DeterministicExecutor, ManualClock, IDFactory and LeakCensus for each test; objects are reclaimed exactly by drain(). */
class SpinePrototypeTest : public testing::Test {
protected:
	void SetUp() override {
		Destroyed::reset();
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 42);
		executor = backend.get();
		utils::ThreadPoolManager::installBackend(std::move(backend));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::LeakCensus::getInstance().install();
	}

	void TearDown() override {
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		runtime::Reclaimer::getInstance().drain();
	}

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr;
	/** static data double: a default NpcTemplate (no AI name, no stats) */
	static inline const model::templates::npc::NpcTemplate* npcTemplate = new model::templates::npc::NpcTemplate();
	/** static data double: SkillTemplate for Effect */
	static inline const skillengine::model::SkillTemplate* skillTemplate = new skillengine::model::SkillTemplate();
};

#if AION_PROTOTYPE_NPC
TEST_F(SpinePrototypeTest, NpcCreatePostConstructPartsAiEffectPinAndDestruction) {
	Ref<PrototypeNpc> npc;
	Ref<model::templates::spawns::SpawnGroup> group;
	Ref<TestSpawnTemplate> spawnTemplate;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		group = model::templates::spawns::SpawnGroup::create(210010000, 700000, 0, nullptr);
		spawnTemplate = Ref<TestSpawnTemplate>(
			static_cast<TestSpawnTemplate&>(group->addSpawnTemplate(std::make_unique<TestSpawnTemplate>(*group))));
		try {
			npc = VisibleObject::create<PrototypeNpc>(std::make_unique<controllers::NpcController>(), *spawnTemplate, npcTemplate);
		} catch (const runtime::UnportedException& e) {
			GTEST_SKIP() << "VisibleObject::create<Npc> is blocked by an unported body: " << e.what();
		}
	}
	int32_t objectId = 0;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		objectId = npc->getObjectId();
		EXPECT_GT(objectId, 0); // IDFactory::nextId
		EXPECT_EQ(npc->refCount(), 1u) << "postConstruct must not publish the object";

		// late-bound controller: bound to its owner by Npc::postConstruct, narrowed accessors on both sides
		controllers::NpcController& controller = npc->getController();
		EXPECT_EQ(&controller.getOwner(), npc.get());
		EXPECT_EQ(&controller.partOwner(), static_cast<const runtime::RefCounted*>(npc.get()));
		EXPECT_EQ(npc->getObjectTemplate(), npcTemplate);
		EXPECT_EQ(npc->getSpawn().get(), static_cast<model::templates::spawns::SpawnTemplate*>(spawnTemplate.get()));

		// virtual createAggroList() dispatched to the subclass during postConstruct (the reason for two-phase construction)
		EXPECT_NE(dynamic_cast<PrototypeAggroList*>(&npc->getAggroList()), nullptr);
		EXPECT_TRUE(npc->getMoveController());
		EXPECT_TRUE(npc->getGameStats());
		EXPECT_TRUE(npc->getLifeStats());

		// the AI: created by the AION_AI factory for the Npc, a part of the creature, narrowed owner accessor
		auto* ai = dynamic_cast<handlers::ai::prototype::PrototypeNpcAI*>(&npc->getAi());
		ASSERT_NE(ai, nullptr);
		EXPECT_EQ(&ai->getOwner(), npc.get());
		EXPECT_TRUE(ai->isOwnerBound());
		EXPECT_EQ(handlers::ai::prototype::PrototypeNpcAI::live.load(), 1);

		// Ref and Pin of a part retain the owner; Ptr borrows do not
		{
			Ptr<Creature> creature(*npc);
			Ptr<Npc> asNpc = runtime::cast<Npc>(creature);
			EXPECT_EQ(asNpc.get(), npc.get());
			EXPECT_FALSE(runtime::as<model::gameobjects::player::Player>(creature)); // Java instanceof
			EXPECT_EQ(npc->refCount(), 1u);
			Ref<ai::AbstractAI> aiRef(npc->getAi());
			EXPECT_EQ(npc->refCount(), 2u);
			EXPECT_EQ(ai->partRefCount(), 1u);
			runtime::Pin pin(&controller);
			EXPECT_EQ(npc->refCount(), 3u);
		}
		EXPECT_EQ(npc->refCount(), 1u);
		EXPECT_EQ(ai->partRefCount(), 0u);
	}

	// an Effect on the creature: Refs to effector and effected, the effect controller part, ForceType interning
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		ASSERT_TRUE(npc->getEffectController());
		EXPECT_EQ(&npc->getEffectController()->getOwner(), npc.get());
		Ref<skillengine::model::Effect> effect =
			skillengine::model::Effect::create(*npc, Ptr<Creature>(*npc), skillTemplate, 1, std::nullopt, skillengine::model::Effect::ForceType::DEFAULT);
		EXPECT_EQ(effect->getEffector().get(), npc.get());
		EXPECT_EQ(effect->getSkillTemplate(), skillTemplate);
		EXPECT_EQ(npc->refCount(), 3u); // effector + effected
		Ref<model::stats::calc::StatOwner> statOwner(*effect);
		EXPECT_EQ(effect->refCount(), 2u);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(npc->refCount(), 1u) << "the released effect gave back its references";

	// a task pinning the AI part keeps the creature alive; //ai set while it is pending keeps the old AI alive for the task
	static std::atomic<int32_t> ranWithOwner{0};
	ranWithOwner = 0;
	runtime::FutureRef task;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		ai::AbstractAI* oldAi = &npc->getAi();
		task = utils::ThreadPoolManager::getInstance().schedule(runtime::Pin(oldAi), [oldAi, objectId] {
			if (oldAi->getOwner().getObjectId() == objectId)
				ranWithOwner.fetch_add(1);
		}, 1000);
		EXPECT_EQ(npc->refCount(), 2u);
		npc->replaceAi(std::make_unique<handlers::ai::prototype::PrototypeNpcAI>(*npc));
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*npc, "Npc", objectId);
	}
	npc.reset();
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(Destroyed::npcs.load(), 0) << "the pending task pins the AI part and with it the Npc";
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 1u);

	executor->advance(std::chrono::milliseconds(1000));
	EXPECT_EQ(ranWithOwner.load(), 1);
	task.reset();
	runtime::Reclaimer::getInstance().drain();

	// destruction: the Npc and every part are freed, the census forgets it and reports nothing
	EXPECT_EQ(Destroyed::npcs.load(), 1);
	EXPECT_EQ(Destroyed::aggroLists.load(), 1);
	EXPECT_EQ(Destroyed::knownLists.load(), 1);
	EXPECT_EQ(handlers::ai::prototype::PrototypeNpcAI::live.load(), 0) << "the replaced AI is destroyed with its owner";
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u);
	EXPECT_TRUE(runtime::LeakCensus::getInstance().getLeaks().empty());
	EXPECT_EQ(runtime::LeakCensus::getInstance().zombieCutCount(), 0u);
	spawnTemplate.reset();
	group.reset();
}
#endif

#endif // AION_PROTOTYPE_CREATURE

} // namespace
} // namespace aion::gameserver::prototype
