// The registry tables that aion_gs_add_registries() generated from the fixture handler tree (tests/fixtures/valid), linked with the fixture
// handler library: entries, sorting, factories and the report.

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "regscan_fixture/FakeCore.h"

namespace gs = aion::gameserver;
using namespace aion::gameserver::handlers;

TEST(GeneratedRegistryTest, AIHandlers) {
	std::span<const AIHandlerEntry> entries = aiHandlerEntries();
	ASSERT_EQ(entries.size(), 4u);
	EXPECT_TRUE(std::ranges::is_sorted(entries, {}, &AIHandlerEntry::name));
	EXPECT_EQ(entries[0].name, "aggressive");
	EXPECT_EQ(entries[0].javaClass, "ai.AggressiveNpcAI");
	EXPECT_EQ(entries[0].source.substr(0, 23), "ai/AggressiveNpcAI.cpp:");

	const AIHandlerEntry* darkPoeta = findAIHandler(entries, "calindi_flamelord");
	const AIHandlerEntry* dragonLords = findAIHandler(entries, "calindi_flamelord_drl");
	ASSERT_NE(darkPoeta, nullptr);
	ASSERT_NE(dragonLords, nullptr);
	EXPECT_EQ(darkPoeta->javaClass, "ai.instance.darkPoeta.CalindiFlamelordAI");
	EXPECT_EQ(dragonLords->javaClass, "ai.instance.dragonLordsRefuge.CalindiFlamelordAI");
	EXPECT_NE(darkPoeta->create, dragonLords->create); // same simple class name, different namespaces

	gs::model::gameobjects::Npc npc("Calindi");
	gs::model::gameobjects::Summon summon("Summon");
	auto ai = darkPoeta->create(npc);
	ASSERT_NE(ai, nullptr);
	EXPECT_EQ(ai->describe(), "calindi_flamelord:Calindi");
	EXPECT_EQ(findAIHandler(entries, "aggressive")->create(npc)->describe().substr(0, 19), "aggressive:Calindi ");
	EXPECT_EQ(findAIHandler(entries, "general")->create(summon), nullptr); // owner type mismatch
	EXPECT_EQ(findAIHandler(entries, "not_ported"), nullptr);
}

TEST(GeneratedRegistryTest, InstanceZoneQuestHandlers) {
	{
		gs::runtime::TaskScope scope(AION_TASK_INFO(gs::runtime::TaskKind::TEST));
		std::span<const InstanceHandlerEntry> instances = instanceHandlerEntries();
		ASSERT_EQ(instances.size(), 1u);
		const InstanceHandlerEntry* dredgion = findInstanceHandler(instances, 300110000);
		ASSERT_NE(dredgion, nullptr);
		EXPECT_EQ(dredgion->javaClass, "instance.BaranathDredgionInstance");
		gs::world::WorldMapInstance map(300110000);
		auto handler = dredgion->create(map);
		ASSERT_NE(handler, nullptr);
		EXPECT_EQ(handler->getMapId(), 300110000);

		std::span<const ZoneHandlerEntry> zones = zoneHandlerEntries();
		ASSERT_EQ(zones.size(), 2u);
		EXPECT_EQ(zones[0].zoneNames, "LC1_PVP DC1_PVP");
		EXPECT_EQ(zones[0].questId, 0);
		EXPECT_EQ(zones[0].javaClass, "zone.pvpZones.PvPAreaZone");
		EXPECT_EQ(zones[1].zoneNames, "LF1A_A LF1A_B LF1A_C");
		EXPECT_EQ(zones[1].questId, 1012);
		EXPECT_EQ(zoneNamesOf(zones[1]), (std::vector<std::string_view>{"LF1A_A", "LF1A_B", "LF1A_C"}));
		EXPECT_EQ(zones[1].create(zones[1].questId)->getQuestId(), 1012);
		EXPECT_EQ(zones[0].create(zones[0].questId)->getQuestId(), 0);
	}
	gs::runtime::Reclaimer::getInstance().reclaimNow();

	std::span<const QuestHandlerEntry> quests = questHandlerEntries();
	ASSERT_EQ(quests.size(), 2u);
	EXPECT_EQ(quests[0].questId, 1500);
	EXPECT_EQ(quests[1].questId, 9001);
	EXPECT_EQ(quests[1].javaClass, "quest.template._9001KeywordPackage");
	for (const QuestHandlerEntry& entry : quests)
		EXPECT_EQ(entry.create()->getQuestId(), entry.questId); // the QuestEngine consistency check
}

TEST(GeneratedRegistryTest, CommandsAndClientPackets) {
	std::span<const CommandEntry> commands = commandEntries();
	ASSERT_EQ(commands.size(), 3u);
	EXPECT_EQ(commands[0].kind, CommandKind::ADMIN);
	EXPECT_EQ(commands[0].javaClass, "admincommands.Add");
	EXPECT_EQ(commands[1].kind, CommandKind::PLAYER);
	EXPECT_EQ(commands[2].kind, CommandKind::CONSOLE);
	EXPECT_EQ(commands[2].create()->getAlias(), "attrbonus");

	std::span<const ClientPacketEntry> packets = clientPacketEntries();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(packets[0].name, "CM_MOVE");
	EXPECT_EQ(packets[1].name, "CM_QUIT");
	EXPECT_EQ(packets[1].source.substr(0, 38), "network/aion/clientpackets/CM_QUIT.cpp");
	auto move = findClientPacket(packets, "CM_MOVE")->create(0, gs::network::aion::StateSet(4));
	EXPECT_EQ(move->getOpCode(), 48);
	EXPECT_EQ(move->getStateBits(), 4);
}

TEST(GeneratedRegistryTest, NpcIdsSpawnedByHandlers) {
	std::span<const int32_t> ids = npcIdsSpawnedByHandlers();
	EXPECT_EQ(std::vector<int32_t>(ids.begin(), ids.end()), (std::vector<int32_t>{206001, 206002, 215074, 299999, 700001, 900001}));
}

TEST(GeneratedRegistryTest, Report) {
	std::ifstream in(AION_REGSCAN_FIXTURE_REPORT, std::ios::binary);
	ASSERT_TRUE(in.is_open()) << AION_REGSCAN_FIXTURE_REPORT;
	std::ostringstream content;
	content << in.rdbuf();
	std::string report = content.str();
	EXPECT_NE(report.find("registry\tported\tjava\tmissing\tunknownToJava\n"
						  "ai\t4\t5\t1\t0\n"
						  "instance\t1\t2\t1\t0\n"
						  "zone names\t5\t5\t0\t0\n"
						  "quest\t2\t2\t1\t1\n"
						  "admin commands\t1\t2\t1\t0\n"
						  "player commands\t1\t1\t0\t0\n"
						  "console commands\t1\t1\t0\t0\n"
						  "client packets\t2\t3\t1\t0\n"
						  "npc ids spawned by handlers\t6\t6\t1\t1\n"),
		std::string::npos)
		<< report;
	EXPECT_NE(report.find("## ai: missing (1)\nnot_ported\tai.NotPortedAI\n"), std::string::npos) << report;
}
