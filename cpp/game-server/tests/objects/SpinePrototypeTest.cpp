// Spine prototype against the real hub headers (handlers-and-porting-plan.md "Risks"; hub-headers.md §3.5 freeze gate 3): create<T>/postConstruct
// with the AI created by AIEngine, one Effect, a part pinned by a scheduled task, destruction with an empty LeakCensus, and one handler of
// every registry kind created by its registry factory.
//
// What runs for real: VisibleObject::create<Npc> with Npc's constructor (IDFactory id, WorldPosition, NpcSkillList reading
// DataManager.NPC_SKILL_DATA) and the postConstruct chain (Creature: AIEngine::newAI with the template/spawn AI name, the virtual
// createAggroList; Npc: late-bound controller owner, NpcMoveController, the virtual setupStatContainers), part ownership (OwnedPart bound to
// the creature, Ref<Part> and Pin retain the owner), Ref/Ptr use, an Effect holding Refs to its creatures, a task pinning the AI part while
// `//ai set` replaces it, destruction through the Reclaimer. The handlers: an AI (AION_AI factory, installed like `//ai set`), a quest
// handler and a quest zone handler reading DataManager.QUEST_DATA through QuestsData::getQuestById, a general zone handler held by
// Ref<ZoneHandler>, and the admin, player and console commands. The instance handler is checked at the factory type only (see below).
//
// Test doubles, each standing in for a body of a later chunk, never for spine code:
// - static data: a default NpcTemplate/SkillTemplate, an empty NpcSkillData and a QuestsData bound from XML text, published into DataManager;
// - stat containers: PrototypeNpc overrides the virtual setupStatContainers (Java protected) with the real NpcGameStats and a life stats part
//   with fixed HP/MP, because NpcLifeStats reads CreatureGameStats::getMaxHp (the stat calculation of P5-01);
// - the AI registry of this test executable is the empty table (aion_gs_registry_empty): AIEngine::newAI creates the DummyAI for an npc
//   template without an AI name and rejects unknown names like Java; the handler AI is installed through its AION_AI factory.
// Player: the account side runs for real (Account, PlayerCommonData, PlayerAppearance, the PlayerAccountData part with its interned run-time
// BoundRadius); create<Player> itself is a freeze exception (spine-status.md): its constructor creates PetList, whose Java constructor loads
// the pets through PlayerPetsDAO (P4-14), and postConstruct creates PlayerGameStats/PlayerLifeStats (static data and the stat calculation,
// P5-01). None of them is a Java override point a test double could replace; the scenario checks that creation stops at the PetList body.
// Not created here: an instance handler needs a WorldMapInstance, whose construction is P4-10 world code (WorldMap's constructor,
// WorldMapInstanceFactory, WorldMap2DInstance/WorldMap3DInstance and world/zone/ZoneService).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "aion/gameserver/ai/AIEngine.h"
#include "aion/gameserver/ai/AITemplate.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
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

namespace aion::gameserver::prototype {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using runtime::Ptr;
using runtime::Ref;

// ---------------------------------------------------------------------------------------------------------------------------------------
// Compile-time shape checks
// ---------------------------------------------------------------------------------------------------------------------------------------

// Construction goes only through VisibleObject::create<T>: no public constructor without the CreateKey passkey.
static_assert(!std::is_constructible_v<Npc, std::unique_ptr<controllers::NpcController>, model::templates::spawns::SpawnTemplate&,
	const model::templates::npc::NpcTemplate*>);
static_assert(!std::is_constructible_v<model::gameobjects::player::Player, model::account::PlayerAccountData&, model::account::Account&>);
// Late-bound controllers and AI are parts (OwnedPart): Ref<part> / Pin(part) retain the owning creature.
static_assert(std::derived_from<controllers::VisibleObjectController, runtime::OwnedPart>);
static_assert(std::derived_from<ai::AbstractAI, runtime::OwnedPart> && std::derived_from<controllers::attack::AggroList, runtime::OwnedPart>);
static_assert(std::derived_from<controllers::effect::EffectController, runtime::OwnedPart>);
static_assert(std::derived_from<world::knownlist::KnownList, runtime::OwnedPart>);
static_assert(std::derived_from<model::skill::NpcSkillList, runtime::OwnedPart>);
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
// NpcAI is abstract in Java: its constructor is protected, so the registry cannot create it (AIHandlerClass needs a public constructor).
static_assert(!handlers::detail::AIHandlerClass<ai::NpcAI>);
// AIEngine::newAI returns the new part for Creature's ai slot.
static_assert(std::same_as<decltype(ai::AIEngine::getInstance().newAI(std::nullopt, std::declval<Creature&>())), std::unique_ptr<ai::AbstractAI>>);

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
AION_AI(PrototypeNpcAI, "prototype_npc");

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
static_assert(std::is_same_v<decltype(PrototypeInstance_instanceFactory), handlers::InstanceFactory>);

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
AION_ZONE_HANDLER(PrototypeQuestArea, "PROTOTYPE_QUEST_AREA_210010000", 1012);

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

	/** test access to the protected members AbstractQuestHandler's constructor loads from the quest template */
	runtime::Ptr<runtime::RcArrayList<const model::templates::quest::QuestItems*>> workItemList() const { return workItems.get(); }
	runtime::Ptr<runtime::RcHashSet<int32_t>> actionItemSet() const { return actionItems.get(); }
};
static_assert(handlers::detail::QuestHandlerClass<_1500PrototypeQuest>);
AION_QUEST_HANDLER(_1500PrototypeQuest, 1500);

/** A quest handler of an artificial quest id without a template (Java: "Some artificial quests have dummy questIds") */
class _1501PrototypeQuest final : public questEngine::handlers::AbstractQuestHandler {
public:
	_1501PrototypeQuest() : AbstractQuestHandler(1501) {}

	void register_() override {}

	bool loadedNothing() const { return !workItems.get() && !actionItems.get(); }
};
AION_QUEST_HANDLER(_1501PrototypeQuest, 1501);

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

// ---------------------------------------------------------------------------------------------------------------------------------------
// Static data doubles
// ---------------------------------------------------------------------------------------------------------------------------------------

/** Quest templates for the quest handler (work items, one action item drop of a 7xxxxx object, one ordinary drop) and the quest zone */
constexpr std::string_view QUEST_DATA_XML = R"(<quests>
	<quest id="1500" name="Prototype Quest" nameId="1103000">
		<quest_drop npc_id="700157" item_id="182201001" drop_each_member="1" collecting_step="3"/>
		<quest_drop npc_id="210671" item_id="182200001" drop_each_member="1" collecting_step="7"/>
		<quest_work_items>
			<quest_work_item item_id="182206058"/>
			<quest_work_item item_id="182206062"/>
		</quest_work_items>
	</quest>
	<quest id="1012" name="Prototype Zone Quest" nameId="1102012"/>
</quests>)";

/** Publishes the static data holders the spine constructors read (DataManager.QUEST_DATA, NPC_SKILL_DATA) and forgets them again. */
class StaticDataDoubles {
public:
	StaticDataDoubles() {
		xml::LoadContext context;
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_DATA_XML, "prototype_quest_data.xml"));
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
	}
	~StaticDataDoubles() {
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}
	StaticDataDoubles(const StaticDataDoubles&) = delete;
	StaticDataDoubles& operator=(const StaticDataDoubles&) = delete;
};

// ---------------------------------------------------------------------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------------------------------------------------------------------

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

TEST(SpinePrototypeHandlers, QuestZoneHandlerChecksItsQuestInQuestsData) {
	StaticDataDoubles data;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		// the registry passes the marker's quest id (Java: @ZoneNameAnnotation(questId = 1012))
		Ref<world::zone::handler::ZoneHandler> handler = handlers::zone::prototype::PrototypeQuestArea_zoneFactory(1012);
		ASSERT_TRUE(handler);
		EXPECT_NE(dynamic_cast<world::zone::handler::QuestZoneHandler*>(handler.get()), nullptr);
		// Java: IncompleteAnnotationException for a missing quest id or a quest without a template
		EXPECT_THROW(static_cast<void>(handlers::zone::prototype::PrototypeQuestArea_zoneFactory(0)), runtime::IllegalStateException);
		EXPECT_THROW(static_cast<void>(handlers::zone::prototype::PrototypeQuestArea_zoneFactory(1013)), runtime::IllegalStateException);
	}
	runtime::Reclaimer::getInstance().drain();
}

TEST(SpinePrototypeHandlers, QuestHandlerFactoryLoadsWorkAndActionItemsThroughQuestsData) {
	StaticDataDoubles data;
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	ASSERT_NE(dataholders::DataManager::QUEST_DATA->getQuestById(1500), nullptr);

	std::unique_ptr<questEngine::handlers::AbstractQuestHandler> handler = handlers::quest::prototype::_1500PrototypeQuest_questFactory();
	ASSERT_TRUE(handler);
	EXPECT_EQ(handler->getQuestId(), 1500);
	auto* quest = dynamic_cast<handlers::quest::prototype::_1500PrototypeQuest*>(handler.get());
	ASSERT_NE(quest, nullptr);
	Ptr<runtime::RcArrayList<const model::templates::quest::QuestItems*>> workItems = quest->workItemList();
	ASSERT_TRUE(workItems);
	ASSERT_EQ(workItems->size(), 2);
	EXPECT_EQ(workItems->get(0)->getItemId(), 182206058);
	EXPECT_EQ(workItems->get(1)->getItemId(), 182206062);
	// only drops of 7xxxxx objects are action items (Java: drop.getNpcId() / 100000 == 7)
	Ptr<runtime::RcHashSet<int32_t>> actionItems = quest->actionItemSet();
	ASSERT_TRUE(actionItems);
	EXPECT_EQ(actionItems->size(), 1);
	EXPECT_TRUE(actionItems->contains(700157));

	std::unique_ptr<questEngine::handlers::AbstractQuestHandler> dummy = handlers::quest::prototype::_1501PrototypeQuest_questFactory();
	EXPECT_TRUE(dynamic_cast<handlers::quest::prototype::_1501PrototypeQuest&>(*dummy).loadedNothing());
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

// ---------------------------------------------------------------------------------------------------------------------------------------
// Npc: test doubles
// ---------------------------------------------------------------------------------------------------------------------------------------

/** Counts destroyed parts to prove the owner's destructor frees them and that virtual createAggroList() dispatched to the subclass. */
struct Destroyed {
	static inline std::atomic<int32_t> npcs{0};
	static inline std::atomic<int32_t> aggroLists{0};
	static inline std::atomic<int32_t> knownLists{0};
	static inline std::atomic<int32_t> lifeStats{0};

	static void reset() {
		npcs = 0;
		aggroLists = 0;
		knownLists = 0;
		lifeStats = 0;
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

/** Life stats with the HP/MP a static data load would give (NpcLifeStats reads them from the P5-01 stat calculation). */
class PrototypeLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	PrototypeLifeStats(Creature& owner, int32_t currentHp, int32_t currentMp) : CreatureLifeStats(owner, currentHp, currentMp) {}
	~PrototypeLifeStats() override { Destroyed::lifeStats.fetch_add(1); }
};

/**
 * An Npc with the real constructor and postConstruct chain; the virtual hooks Java subclasses override are the only test doubles: the aggro
 * list (destruction counter), the stat containers (static data) and the known list SpawnEngine would set before spawning.
 */
class PrototypeNpc final : public Npc {
	AION_MAKE_REF_FRIEND
public:
	PrototypeNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, model::templates::spawns::SpawnTemplate& spawnTemplate,
		const model::templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~PrototypeNpc() override { Destroyed::npcs.fetch_add(1); }

	std::unique_ptr<controllers::attack::AggroList> createAggroList() override { return std::make_unique<PrototypeAggroList>(*this); }

	void setupStatContainers() override {
		setGameStats(std::make_unique<model::stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<PrototypeLifeStats>(*this, 1200, 300));
	}

	void postConstruct() override {
		Npc::postConstruct();
		setKnownlist(std::make_unique<PrototypeKnownList>(*this)); // Java: SpawnEngine sets the known list before spawning
	}
};

/** A spawn template part of its group (SpawnTemplate is an OwnedPart of SpawnGroup) */
class TestSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	TestSpawnTemplate(model::templates::spawns::SpawnGroup& group, std::optional<std::string_view> aiName)
		: SpawnTemplate(group, 1.5f, 2.5f, 3.5f, int8_t{30}, 0, std::nullopt, 0, 0, aiName) {}
};

/** A spawn group with its one spawn template (Java: the SpawnTemplate constructor adds itself to the group) */
struct TestSpawn {
	Ref<model::templates::spawns::SpawnGroup> group;
	Ref<TestSpawnTemplate> spawnTemplate;

	explicit TestSpawn(std::optional<std::string_view> aiName)
		: group(model::templates::spawns::SpawnGroup::create(210010000, 700000, 0, nullptr)),
		  spawnTemplate(static_cast<TestSpawnTemplate&>(group->addSpawnTemplate(std::make_unique<TestSpawnTemplate>(*group, aiName)))) {}
};

/** DeterministicExecutor, ManualClock, IDFactory, LeakCensus and the static data doubles for each test; objects are reclaimed by drain(). */
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
		staticData = std::make_unique<StaticDataDoubles>();
	}

	void TearDown() override {
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		runtime::Reclaimer::getInstance().drain();
		staticData.reset();
	}

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr;
	std::unique_ptr<StaticDataDoubles> staticData;
	/** static data double: a default NpcTemplate (id 0, no AI name, no skills in the published NpcSkillData) */
	static inline const model::templates::npc::NpcTemplate* npcTemplate = new model::templates::npc::NpcTemplate();
	/** static data double: SkillTemplate for Effect */
	static inline const skillengine::model::SkillTemplate* skillTemplate = new skillengine::model::SkillTemplate();
};

// ---------------------------------------------------------------------------------------------------------------------------------------
// Npc scenario
// ---------------------------------------------------------------------------------------------------------------------------------------

TEST_F(SpinePrototypeTest, NpcCreatePostConstructPartsAiEffectPinAndDestruction) {
	Ref<PrototypeNpc> npc;
	TestSpawn spawn(std::nullopt);
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		npc = VisibleObject::create<PrototypeNpc>(std::make_unique<controllers::NpcController>(), *spawn.spawnTemplate, npcTemplate);
	}
	int32_t objectId = 0;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		objectId = npc->getObjectId();
		EXPECT_GT(objectId, 0); // IDFactory::nextId
		EXPECT_EQ(npc->refCount(), 1u) << "postConstruct must not publish the object";
		EXPECT_EQ(npc->getNpcId(), 0);
		EXPECT_TRUE(npc->getPosition()) << "a new WorldPosition of the spawn's world";

		// late-bound controller: bound to its owner by Npc::postConstruct, narrowed accessors on both sides
		controllers::NpcController& controller = npc->getController();
		EXPECT_EQ(&controller.getOwner(), npc.get());
		EXPECT_EQ(&controller.partOwner(), static_cast<const runtime::RefCounted*>(npc.get()));
		EXPECT_EQ(npc->getObjectTemplate(), npcTemplate);
		EXPECT_EQ(npc->getSpawn().get(), static_cast<model::templates::spawns::SpawnTemplate*>(spawn.spawnTemplate.get()));

		// parts created by the constructor and postConstruct: the skill list from NPC_SKILL_DATA (no entry: empty), the virtual
		// createAggroList() dispatched to the subclass (the reason for two-phase construction), move controller and stat containers
		Ptr<model::skill::NpcSkillList> skills = npc->getSkillList();
		ASSERT_TRUE(skills);
		ASSERT_TRUE(skills->getNpcSkills());
		EXPECT_TRUE(skills->getNpcSkills()->isEmpty());
		EXPECT_FALSE(skills->getPriorities());
		EXPECT_EQ(&skills->partOwner(), static_cast<const runtime::RefCounted*>(npc.get()));
		EXPECT_NE(dynamic_cast<PrototypeAggroList*>(&npc->getAggroList()), nullptr);
		EXPECT_TRUE(npc->getMoveController());
		EXPECT_TRUE(npc->getGameStats());
		// the life stats double is a CreatureLifeStats (Npc::getLifeStats would throw ClassCastException like Java's cast)
		ASSERT_TRUE(npc->Creature::getLifeStats());
		EXPECT_EQ(npc->Creature::getLifeStats()->getCurrentHp(), 1200);

		// the AI: created by AIEngine::newAI in Creature::postConstruct. The template has no AI name, so it is the DummyAI (no registry
		// entry), a part of the creature bound to it
		ai::AbstractAI& createdAi = npc->getAi();
		EXPECT_EQ(createdAi.getRegistryEntry(), nullptr);
		EXPECT_TRUE(createdAi.isOwnerBound());
		EXPECT_EQ(&createdAi.partOwner(), static_cast<const runtime::RefCounted*>(npc.get()));
		EXPECT_EQ(dynamic_cast<ai::NpcAI*>(&createdAi), nullptr);

		// `//ai set prototype_npc`: the AION_AI factory creates the handler AI for the Npc, replaceAi keeps the previous AI with the creature
		npc->replaceAi(handlers::ai::prototype::PrototypeNpcAI_aiFactory(*npc));
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
	EXPECT_EQ(Destroyed::lifeStats.load(), 1);
	EXPECT_EQ(handlers::ai::prototype::PrototypeNpcAI::live.load(), 0) << "the replaced AIs are destroyed with their owner";
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u);
	EXPECT_TRUE(runtime::LeakCensus::getInstance().getLeaks().empty());
	EXPECT_EQ(runtime::LeakCensus::getInstance().zombieCutCount(), 0u);
}

TEST_F(SpinePrototypeTest, NpcAiNameOfTheSpawnSelectsTheAiThroughAIEngine) {
	// Java Creature constructor: the spawn's AI name overrides the template's; AIEngine.newAI throws for a name without an AI handler
	{
		TestSpawn spawn("prototype_npc");
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		try {
			static_cast<void>(VisibleObject::create<PrototypeNpc>(std::make_unique<controllers::NpcController>(), *spawn.spawnTemplate, npcTemplate));
			ADD_FAILURE() << "the empty AI registry of this test executable has no AI named prototype_npc";
		} catch (const runtime::IllegalArgumentException& e) {
			EXPECT_STREQ(e.what(), "No AI found for name prototype_npc");
		}
	}
	runtime::Reclaimer::getInstance().drain();
	// the Npc whose postConstruct threw (in Creature::postConstruct, before the aggro list) is released and destroyed with its parts
	EXPECT_EQ(Destroyed::npcs.load(), 1);
	EXPECT_EQ(Destroyed::aggroLists.load(), 0) << "the aggro list is created after the AI";

	// SpawnTemplate.NO_AI disables the AI name: the DummyAI
	{
		TestSpawn spawn(model::templates::spawns::SpawnTemplate::NO_AI);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<PrototypeNpc> npc = VisibleObject::create<PrototypeNpc>(std::make_unique<controllers::NpcController>(), *spawn.spawnTemplate, npcTemplate);
		EXPECT_EQ(npc->getAi().getRegistryEntry(), nullptr);
		EXPECT_EQ(&npc->getAi().partOwner(), static_cast<const runtime::RefCounted*>(npc.get()));
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(Destroyed::npcs.load(), 2);
	EXPECT_EQ(Destroyed::aggroLists.load(), 1);
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Player scenario (account side; create<Player> is a freeze exception, see the file comment)
// ---------------------------------------------------------------------------------------------------------------------------------------

TEST_F(SpinePrototypeTest, PlayerAccountDataIsAPartOfItsAccountAndInternsItsBoundRadius) {
	using model::account::Account;
	using model::account::PlayerAccountData;
	using model::gameobjects::player::PlayerAppearance;
	using model::gameobjects::player::PlayerCommonData;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<Account> account = Account::create(7);
		Ref<PlayerCommonData> commonData = PlayerCommonData::create(100);
		Ref<PlayerAppearance> appearance = PlayerAppearance::create();
		appearance->setHeight(1.0f);

		// Java: new PlayerAccountData(playerCommonData, appearance); account.addPlayerAccountData(data)
		account->addPlayerAccountData(std::make_unique<PlayerAccountData>(*account, *commonData, *appearance));
		Ptr<PlayerAccountData> data = account->getPlayerAccountData(100);
		ASSERT_TRUE(data);
		EXPECT_FALSE(account->getPlayerAccountData(101));
		EXPECT_EQ(&data->partOwner(), static_cast<const runtime::RefCounted*>(account.get()));
		EXPECT_EQ(data->getPlayerCommonData().get(), commonData.get());

		// updateBoundingRadius: Java new BoundRadius(0.25f, 0.25f, appearance.getBoundHeight()), interned as an immortal
		const model::templates::BoundRadius* radius = commonData->getBoundRadius();
		ASSERT_NE(radius, nullptr);
		EXPECT_FLOAT_EQ(radius->getFront(), 0.25f);
		EXPECT_FLOAT_EQ(radius->getSide(), 0.25f);
		EXPECT_FLOAT_EQ(radius->getUpper(), 1.75f);
		Ref<PlayerCommonData> otherData = PlayerCommonData::create(101);
		account->addPlayerAccountData(std::make_unique<PlayerAccountData>(*account, *otherData, *appearance));
		EXPECT_EQ(otherData->getBoundRadius(), radius) << "equal appearance heights share one interned radius";

		// a Ref to the part retains the account
		EXPECT_EQ(account->refCount(), 1u);
		{
			Ref<PlayerAccountData> dataRef(*data);
			EXPECT_EQ(account->refCount(), 2u);
		}
		EXPECT_EQ(account->refCount(), 1u);

		// create<Player> stops at the PetList constructor (PlayerPetsDAO, P4-14): the freeze exception of this scenario
		EXPECT_THROW(static_cast<void>(VisibleObject::create<model::gameobjects::player::Player>(*data, *account)), runtime::UnportedException);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::prototype
