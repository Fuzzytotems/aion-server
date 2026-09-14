// Markers, concepts and factories of HandlerRegistry.h against the stand-in core classes of FakeCore.h. The generated tables themselves are
// tested by aion_gs_regscan's integration tests (game-server/tools/regscan/tests).

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>

#include "FakeCore.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

using aion::gameserver::model::gameobjects::Creature;
using aion::gameserver::model::gameobjects::Npc;
using aion::gameserver::model::gameobjects::Summon;
using aion::gameserver::runtime::Ref;

// ---------------------------------------------------------------------------------------------------------------------------------------
// handlers, written as handler files will be (one marker line after the class, in the package namespace)
// ---------------------------------------------------------------------------------------------------------------------------------------

namespace aion::gameserver::handlers::ai {
class TestNpcAI : public gameserver::ai::AITemplate<model::gameobjects::Npc> {
public:
	explicit TestNpcAI(model::gameobjects::Npc& owner) : AITemplate(owner) {}
	std::string describe() const override { return "npc ai of " + getOwner().name; }
};
AION_AI(TestNpcAI, "test_npc");

class TestCreatureAI final : public gameserver::ai::AITemplate<model::gameobjects::Creature> {
public:
	explicit TestCreatureAI(model::gameobjects::Creature& owner) : AITemplate(owner) {}
	std::string describe() const override { return "creature ai of " + getOwner().name; }
};
AION_AI(TestCreatureAI, "test_creature");
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::handlers::instance {
class TestInstance : public gameserver::instance::handlers::InstanceHandler {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<TestInstance> create(world::WorldMapInstance& instance) { return runtime::makeRef<TestInstance>(instance); }
	int32_t getMapId() const override { return mapId; }

protected:
	explicit TestInstance(world::WorldMapInstance& instance) : mapId(instance.mapId) {}
	~TestInstance() override = default;

private:
	const int32_t mapId;
};
AION_INSTANCE_HANDLER(TestInstance, 300110000);
} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::zone {
class _1012TestArea final : public world::zone::handler::QuestZoneHandler {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<_1012TestArea> create(int32_t questId) { return runtime::makeRef<_1012TestArea>(questId); }

protected:
	explicit _1012TestArea(int32_t questId) : QuestZoneHandler(questId) {}
	~_1012TestArea() override = default;
};
AION_ZONE_HANDLER(_1012TestArea, "ZONE_A ZONE_B", 1012);
} // namespace aion::gameserver::handlers::zone

namespace aion::gameserver::handlers::zone::pvpZones {
class TestPvPZone final : public world::zone::handler::ZoneHandler {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<TestPvPZone> create() { return runtime::makeRef<TestPvPZone>(); }

protected:
	TestPvPZone() = default;
	~TestPvPZone() override = default;
};
AION_ZONE_HANDLER(TestPvPZone, "PVP_ZONE");
} // namespace aion::gameserver::handlers::zone::pvpZones

namespace aion::gameserver::handlers::quest::heiron {
class _1500TestQuest final : public questEngine::handlers::AbstractQuestHandler {
public:
	_1500TestQuest() : AbstractQuestHandler(1500) {}
};
AION_QUEST_HANDLER(_1500TestQuest, 1500);
} // namespace aion::gameserver::handlers::quest::heiron

namespace aion::gameserver::handlers::admincommands {
class Add final : public utils::chathandlers::AdminCommand {
public:
	Add() : AdminCommand("add") {}
};
AION_ADMIN_COMMAND(Add);
} // namespace aion::gameserver::handlers::admincommands

namespace aion::gameserver::handlers::playercommands {
class Id final : public utils::chathandlers::PlayerCommand {
public:
	Id() : PlayerCommand("id") {}
};
AION_PLAYER_COMMAND(Id);
} // namespace aion::gameserver::handlers::playercommands

namespace aion::gameserver::handlers::consolecommands {
class Attrbonus final : public utils::chathandlers::ConsoleCommand {
public:
	Attrbonus() : ConsoleCommand("attrbonus") {}
};
AION_CONSOLE_COMMAND(Attrbonus);
} // namespace aion::gameserver::handlers::consolecommands

namespace aion::gameserver::network::aion::clientpackets {
class CM_TEST final : public AionClientPacket {
public:
	CM_TEST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {}
};
AION_CLIENT_PACKET(CM_TEST);
} // namespace aion::gameserver::network::aion::clientpackets

// ---------------------------------------------------------------------------------------------------------------------------------------
// concept checks (what the markers reject at compile time)
// ---------------------------------------------------------------------------------------------------------------------------------------

namespace {

namespace detail = aion::gameserver::handlers::detail;
namespace gs = aion::gameserver;

class AbstractTestAI : public gs::ai::AITemplate<Npc> {
public:
	using AITemplate::AITemplate;
};
class NoOwnerTypeAI : public gs::ai::AbstractAI {
public:
	explicit NoOwnerTypeAI(Npc&) {}
	std::string describe() const override { return ""; }
};
class WrongConstructorAI : public gs::ai::AITemplate<Npc> {
public:
	WrongConstructorAI(Npc& npc, int) : AITemplate(npc) {}
	std::string describe() const override { return ""; }
};
class NotAnAI {
public:
	using OwnerType = Npc;
	explicit NotAnAI(Npc&) {}
};

class InheritedCreateInstance final : public gs::handlers::instance::TestInstance {}; // create() returns Ref<TestInstance>, not this class
class QuestWithoutDefaultConstructor final : public gs::questEngine::handlers::AbstractQuestHandler {
public:
	explicit QuestWithoutDefaultConstructor(int32_t id) : AbstractQuestHandler(id) {}
};

} // namespace

static_assert(detail::AIHandlerClass<gs::handlers::ai::TestNpcAI>);
static_assert(detail::AIHandlerClass<gs::handlers::ai::TestCreatureAI>);
static_assert(!detail::AIHandlerClass<AbstractTestAI>);
static_assert(!detail::AIHandlerClass<NoOwnerTypeAI>);
static_assert(!detail::AIHandlerClass<WrongConstructorAI>);
static_assert(!detail::AIHandlerClass<NotAnAI>);
static_assert(detail::InstanceHandlerClass<gs::handlers::instance::TestInstance>);
static_assert(!detail::InstanceHandlerClass<InheritedCreateInstance>);
static_assert(!detail::InstanceHandlerClass<gs::instance::handlers::InstanceHandler>);
static_assert(detail::QuestZoneHandlerClass<gs::handlers::zone::_1012TestArea>);
static_assert(detail::ZoneHandlerClass<gs::handlers::zone::_1012TestArea>);
static_assert(detail::ZoneHandlerClass<gs::handlers::zone::pvpZones::TestPvPZone>);
static_assert(!detail::QuestZoneHandlerClass<gs::handlers::zone::pvpZones::TestPvPZone>);
static_assert(detail::QuestHandlerClass<gs::handlers::quest::heiron::_1500TestQuest>);
static_assert(!detail::QuestHandlerClass<QuestWithoutDefaultConstructor>);
static_assert(detail::CommandClass<gs::handlers::admincommands::Add, gs::utils::chathandlers::AdminCommand>);
static_assert(!detail::CommandClass<gs::handlers::admincommands::Add, gs::utils::chathandlers::PlayerCommand>);
static_assert(detail::ClientPacketClass<gs::network::aion::clientpackets::CM_TEST>);
static_assert(!detail::ClientPacketClass<gs::network::aion::AionClientPacket>); // protected constructor
static_assert(detail::isPositiveIntKey(1500) && !detail::isPositiveIntKey(0) && !detail::isPositiveIntKey(1500L));
static_assert(detail::zoneQuestId() == 0 && detail::zoneQuestId(1012) == 1012);

// the factories have exactly the registry's function types (the generated tables declare them that way)
static_assert(std::is_same_v<decltype(gs::handlers::ai::TestNpcAI_aiFactory), gs::handlers::AIFactory>);
static_assert(std::is_same_v<decltype(gs::handlers::instance::TestInstance_instanceFactory), gs::handlers::InstanceFactory>);
static_assert(std::is_same_v<decltype(gs::handlers::zone::_1012TestArea_zoneFactory), gs::handlers::ZoneFactory>);
static_assert(std::is_same_v<decltype(gs::handlers::quest::heiron::_1500TestQuest_questFactory), gs::handlers::QuestFactory>);
static_assert(std::is_same_v<decltype(gs::handlers::admincommands::Add_commandFactory), gs::handlers::CommandFactory>);
static_assert(std::is_same_v<decltype(gs::network::aion::clientpackets::CM_TEST_clientPacketFactory), gs::handlers::ClientPacketFactory>);

namespace {

auto testTask() {
	return AION_TASK_INFO(aion::gameserver::runtime::TaskKind::TEST);
}

} // namespace

TEST(HandlerMarkersTest, AIFactoryChecksTheOwnerType) {
	Npc npc("Npc1");
	Summon summon("Summon1");

	auto ai = gs::handlers::ai::TestNpcAI_aiFactory(npc);
	ASSERT_NE(ai, nullptr);
	EXPECT_EQ(ai->describe(), "npc ai of Npc1");
	EXPECT_NE(dynamic_cast<gs::handlers::ai::TestNpcAI*>(ai.get()), nullptr);

	// Java: findConstructor(aiClass, owner.getClass()) fails -> the engine throws with aiOwnerMismatchMessage
	EXPECT_EQ(gs::handlers::ai::TestNpcAI_aiFactory(summon), nullptr);

	auto creatureAi = gs::handlers::ai::TestCreatureAI_aiFactory(summon);
	ASSERT_NE(creatureAi, nullptr);
	EXPECT_EQ(creatureAi->describe(), "creature ai of Summon1");

	gs::handlers::AIHandlerEntry entry{"test_npc", "ai.TestNpcAI", &gs::handlers::ai::TestNpcAI_aiFactory, "ai/TestNpcAI.cpp:1"};
	EXPECT_EQ(gs::handlers::aiOwnerMismatchMessage(entry, "Summon"), "class ai.TestNpcAI cannot be instantiated with Summon as the owner");
}

TEST(HandlerMarkersTest, InstanceAndZoneFactoriesCreateRefCountedHandlers) {
	{
		gs::runtime::TaskScope scope(testTask());
		gs::world::WorldMapInstance map(300110000);
		Ref<gs::instance::handlers::InstanceHandler> handler = gs::handlers::instance::TestInstance_instanceFactory(map);
		ASSERT_NE(handler, nullptr);
		EXPECT_EQ(handler->getMapId(), 300110000);

		Ref<gs::world::zone::handler::ZoneHandler> questZone = gs::handlers::zone::_1012TestArea_zoneFactory(1012);
		ASSERT_NE(questZone, nullptr);
		EXPECT_EQ(questZone->getQuestId(), 1012);

		Ref<gs::world::zone::handler::ZoneHandler> pvpZone = gs::handlers::zone::pvpZones::TestPvPZone_zoneFactory(0);
		ASSERT_NE(pvpZone, nullptr);
		EXPECT_EQ(pvpZone->getQuestId(), 0);
		EXPECT_NE(dynamic_cast<gs::handlers::zone::pvpZones::TestPvPZone*>(pvpZone.get()), nullptr);
	}
	gs::runtime::Reclaimer::getInstance().reclaimNow();
}

TEST(HandlerMarkersTest, SingletonFactories) {
	auto quest = gs::handlers::quest::heiron::_1500TestQuest_questFactory();
	ASSERT_NE(quest, nullptr);
	EXPECT_EQ(quest->getQuestId(), 1500);

	auto add = gs::handlers::admincommands::Add_commandFactory();
	auto id = gs::handlers::playercommands::Id_commandFactory();
	auto attrbonus = gs::handlers::consolecommands::Attrbonus_commandFactory();
	EXPECT_EQ(add->getAlias(), "add");
	EXPECT_EQ(id->getAlias(), "id");
	EXPECT_EQ(attrbonus->getAlias(), "attrbonus");
	EXPECT_NE(dynamic_cast<gs::utils::chathandlers::AdminCommand*>(add.get()), nullptr);

	auto packet = gs::network::aion::clientpackets::CM_TEST_clientPacketFactory(48, gs::network::aion::StateSet(0b100));
	EXPECT_EQ(packet->getOpCode(), 48);
	EXPECT_EQ(packet->getStateBits(), 0b100);
}

TEST(HandlerMarkersTest, LookupHelpersUseTheSortedTables) {
	using namespace gs::handlers;
	static constinit const AIHandlerEntry AIS[] = {
		{"a", "ai.A", &ai::TestNpcAI_aiFactory, "ai/A.cpp:1"},
		{"test_creature", "ai.TestCreatureAI", &ai::TestCreatureAI_aiFactory, "ai/TestCreatureAI.cpp:1"},
		{"test_npc", "ai.TestNpcAI", &ai::TestNpcAI_aiFactory, "ai/TestNpcAI.cpp:1"},
	};
	ASSERT_NE(findAIHandler(AIS, "test_npc"), nullptr);
	EXPECT_EQ(findAIHandler(AIS, "test_npc")->javaClass, "ai.TestNpcAI");
	EXPECT_EQ(findAIHandler(AIS, "a")->javaClass, "ai.A");
	EXPECT_EQ(findAIHandler(AIS, "test"), nullptr);
	EXPECT_EQ(findAIHandler(AIS, "zzz"), nullptr);
	EXPECT_EQ(findAIHandler({}, "a"), nullptr);

	static constinit const InstanceHandlerEntry INSTANCES[] = {
		{300110000, "instance.A", &instance::TestInstance_instanceFactory, "instance/A.cpp:1"},
		{300120000, "instance.B", &instance::TestInstance_instanceFactory, "instance/B.cpp:1"},
	};
	EXPECT_EQ(findInstanceHandler(INSTANCES, 300120000)->javaClass, "instance.B");
	EXPECT_EQ(findInstanceHandler(INSTANCES, 300115000), nullptr);

	static constinit const QuestHandlerEntry QUESTS[] = {{1500, "quest.heiron._1500TestQuest", &quest::heiron::_1500TestQuest_questFactory, "x"}};
	EXPECT_NE(findQuestHandler(QUESTS, 1500), nullptr);
	EXPECT_EQ(findQuestHandler(QUESTS, 1501), nullptr);

	static constinit const ClientPacketEntry PACKETS[] = {
		{"CM_A", &gs::network::aion::clientpackets::CM_TEST_clientPacketFactory, "x"},
		{"CM_TEST", &gs::network::aion::clientpackets::CM_TEST_clientPacketFactory, "y"},
	};
	EXPECT_EQ(findClientPacket(PACKETS, "CM_TEST")->source, "y");
	EXPECT_EQ(findClientPacket(PACKETS, "CM_B"), nullptr);
}

TEST(HandlerMarkersTest, ZoneNamesOfSplitsLikeJava) {
	using gs::handlers::ZoneHandlerEntry;
	ZoneHandlerEntry entry{"LC1_PVP_SUB_C_110010000 DC1_PVP_ZONE_120010000", 0, "zone.pvpZones.PvPAreaZone", nullptr, ""};
	auto names = gs::handlers::zoneNamesOf(entry);
	ASSERT_EQ(names.size(), 2u);
	EXPECT_EQ(names[0], "LC1_PVP_SUB_C_110010000");
	EXPECT_EQ(names[1], "DC1_PVP_ZONE_120010000");

	entry.zoneNames = "SINGLE";
	EXPECT_EQ(gs::handlers::zoneNamesOf(entry), std::vector<std::string_view>{"SINGLE"});
	entry.zoneNames = "";
	EXPECT_TRUE(gs::handlers::zoneNamesOf(entry).empty());
}
