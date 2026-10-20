// A1 quests (lane A, 2026-10-16): the quest npc AIs of chunk A1 (game-server/data/handlers/ai/quests) on npcs of a Poeta map instance
// (A1LaneATestSupport.h). The quest dialogs (QuestEngine has no quest scripts here), the spawns into World (the npcs stand in a map instance
// of their own) and the walks are left to the retest.

#include "A1LaneATestSupport.h"

#include <chrono>
#include <memory>

#include "aion/gameserver/handlers/ai/quests/GarnonQ20060AI.h"
#include "aion/gameserver/handlers/ai/quests/InfiltratorsAI.h"
#include "aion/gameserver/handlers/ai/quests/NidalberBalaurAI.h"
#include "aion/gameserver/handlers/ai/quests/QuestStartItemNpcAI.h"
#include "aion/gameserver/handlers/ai/quests/QuestUseNpcAI.h"

namespace aion::gameserver::handlers::ai::quests {
::aion::gameserver::handlers::AIFactory GarnonQ20060AI_aiFactory;
::aion::gameserver::handlers::AIFactory InfiltratorsAI_aiFactory;
::aion::gameserver::handlers::AIFactory NidalberBalaurAI_aiFactory;
::aion::gameserver::handlers::AIFactory QuestStartItemNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory QuestUseNpcAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::quests

namespace aion::gameserver::network::aion::clientpackets::testing::items::a1 {
namespace {

namespace quests = gameserver::handlers::ai::quests;
using model::gameobjects::Npc;

class QuestsA1AiTest : public A1NpcAiTest {
protected:
	std::string rowsXml() override { return a1NpcTemplatesXml({900100, 282913, 204044, 800020}); }
};

TEST_F(QuestsA1AiTest, TheQuestMarkersBuildTheirClasses) {
	Npc& npc = npcAt(900100);
	EXPECT_NE(dynamic_cast<quests::GarnonQ20060AI*>(quests::GarnonQ20060AI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<quests::InfiltratorsAI*>(quests::InfiltratorsAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<quests::NidalberBalaurAI*>(quests::NidalberBalaurAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<quests::QuestStartItemNpcAI*>(quests::QuestStartItemNpcAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<quests::QuestUseNpcAI*>(quests::QuestUseNpcAI_aiFactory(npc).get()), nullptr);
}

class GarnonProbe final : public quests::GarnonQ20060AI {
public:
	using GarnonQ20060AI::GarnonQ20060AI;
	using GarnonQ20060AI::handleDespawned;
	using GarnonQ20060AI::handleSpawned;
};

/** GarnonQ20060AI.java:22-28: a despawn before the three minutes cancels the replacement */
TEST_F(QuestsA1AiTest, ADespawnedGarnonIsNotReplaced) {
	Npc& garnon = npcAt(900100);
	GarnonProbe& ai = install<GarnonProbe>(garnon);
	ai.handleSpawned();
	ai.handleDespawned();
	executor->advance(std::chrono::milliseconds(180000));
	EXPECT_TRUE(garnon.getPosition()->isSpawned()) << "the task was cancelled: no delete";
}

/** GarnonQ20060AI.java:30-38: three minutes after the spawn Garnon is replaced (the spawn of 800020, then the delete) */
TEST_F(QuestsA1AiTest, GarnonIsReplacedAfterThreeMinutes) {
	Npc& garnon = npcAt(900100);
	install<GarnonProbe>(garnon).handleSpawned();
	executor->advance(std::chrono::milliseconds(179999));
	EXPECT_TRUE(garnon.getPosition()->isSpawned());
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(garnon.getPosition()->isSpawned()) << "deleted";
}

class NidalberProbe final : public quests::NidalberBalaurAI {
public:
	using NidalberBalaurAI::NidalberBalaurAI;
	using NidalberBalaurAI::handleSpawned;
};

/** NidalberBalaurAI.java:25-44: outside the two quest maps the balaur stays where it spawned */
TEST_F(QuestsA1AiTest, ANidalberBalaurOutsideItsMapsStays) {
	Npc& balaur = npcAt(900100);
	install<NidalberProbe>(balaur).handleSpawned();
	EXPECT_TRUE(balaur.getPosition()->isSpawned());
	EXPECT_FLOAT_EQ(balaur.getSpawn()->getX(), 102.0f) << "no new spawn position";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items::a1
