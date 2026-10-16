// A1 events (lane A, 2026-10-16): the event AIs of chunk A1 (game-server/data/handlers/ai/events) on npcs of a Poeta map instance
// (A1LaneATestSupport.h). Events are disabled in the kit (disabled_events = *): these cases are their only check. The item rewards (no item
// rows), the buffs (no skill rows), the quest dialogs, the drops and the spawns into World are not covered.

#include "A1LaneATestSupport.h"

#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/handlers/ai/events/AetherBlossomAI.h"
#include "aion/gameserver/handlers/ai/events/BirthdayCakeAI.h"
#include "aion/gameserver/handlers/ai/events/CodeRedNurseAI.h"
#include "aion/gameserver/handlers/ai/events/FakeCakeAI.h"
#include "aion/gameserver/handlers/ai/events/HalloweenPumpkinAI.h"
#include "aion/gameserver/handlers/ai/events/HiddenBirthdayCakeAI.h"
#include "aion/gameserver/handlers/ai/events/RainbowSnakeAI.h"
#include "aion/gameserver/handlers/ai/events/RedNosedGrankerKingAI.h"
#include "aion/gameserver/handlers/ai/events/RvrBossAI.h"
#include "aion/gameserver/handlers/ai/events/RvrGuardAI.h"
#include "aion/gameserver/handlers/ai/events/iceFestival/AtirunerkAI.h"
#include "aion/gameserver/handlers/ai/events/iceFestival/CompletedIceSculptureAI.h"
#include "aion/gameserver/handlers/ai/events/iceFestival/IceChunkAI.h"
#include "aion/gameserver/handlers/ai/events/iceFestival/IceSculptureAI.h"
#include "aion/gameserver/handlers/ai/events/iceFestival/IshrunerkAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

namespace aion::gameserver::handlers::ai::events {
::aion::gameserver::handlers::AIFactory AetherBlossomAI_aiFactory;
::aion::gameserver::handlers::AIFactory BirthdayCakeAI_aiFactory;
::aion::gameserver::handlers::AIFactory CodeRedNurseAI_aiFactory;
::aion::gameserver::handlers::AIFactory FakeCakeAI_aiFactory;
::aion::gameserver::handlers::AIFactory HalloweenPumpkinAI_aiFactory;
::aion::gameserver::handlers::AIFactory HiddenBirthdayCakeAI_aiFactory;
::aion::gameserver::handlers::AIFactory RainbowSnakeAI_aiFactory;
::aion::gameserver::handlers::AIFactory RedNosedGrankerKingAI_aiFactory;
::aion::gameserver::handlers::AIFactory RvrBossAI_aiFactory;
::aion::gameserver::handlers::AIFactory RvrGuardAI_aiFactory;
namespace iceFestival {
::aion::gameserver::handlers::AIFactory AtirunerkAI_aiFactory;
::aion::gameserver::handlers::AIFactory CompletedIceSculptureAI_aiFactory;
::aion::gameserver::handlers::AIFactory IceChunkAI_aiFactory;
::aion::gameserver::handlers::AIFactory IceSculptureAI_aiFactory;
::aion::gameserver::handlers::AIFactory IshrunerkAI_aiFactory;
} // namespace iceFestival
} // namespace aion::gameserver::handlers::ai::events

namespace aion::gameserver::network::aion::clientpackets::testing::items::a1 {
namespace {

namespace ev = gameserver::handlers::ai::events;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;

class EventsA1AiTest : public A1NpcAiTest {
protected:
	std::string rowsXml() override { return a1NpcTemplatesXml({900100, 831435, 832963, 219484, 833516, 833520}); }
};

TEST_F(EventsA1AiTest, TheEventMarkersBuildTheirClasses) {
	Npc& npc = npcAt(900100);
	EXPECT_NE(dynamic_cast<ev::AetherBlossomAI*>(ev::AetherBlossomAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::BirthdayCakeAI*>(ev::BirthdayCakeAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::CodeRedNurseAI*>(ev::CodeRedNurseAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::FakeCakeAI*>(ev::FakeCakeAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::HalloweenPumpkinAI*>(ev::HalloweenPumpkinAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::HiddenBirthdayCakeAI*>(ev::HiddenBirthdayCakeAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::RainbowSnakeAI*>(ev::RainbowSnakeAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::RedNosedGrankerKingAI*>(ev::RedNosedGrankerKingAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::RvrBossAI*>(ev::RvrBossAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::RvrGuardAI*>(ev::RvrGuardAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::iceFestival::AtirunerkAI*>(ev::iceFestival::AtirunerkAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::iceFestival::CompletedIceSculptureAI*>(ev::iceFestival::CompletedIceSculptureAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::iceFestival::IceChunkAI*>(ev::iceFestival::IceChunkAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::iceFestival::IceSculptureAI*>(ev::iceFestival::IceSculptureAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ev::iceFestival::IshrunerkAI*>(ev::iceFestival::IshrunerkAI_aiFactory(npc).get()), nullptr);
}

/** BirthdayCakeAI.java:22-31, RainbowSnakeAI.java:36-38, CompletedIceSculptureAI.java:29-37: only the first choice is theirs */
TEST_F(EventsA1AiTest, TheBuffNpcsAnswerOnlyTheirFirstChoice) {
	EXPECT_FALSE(install<ev::BirthdayCakeAI>(npcAt(900100)).onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_FALSE(install<ev::RainbowSnakeAI>(npcAt(832963)).onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_FALSE(install<ev::iceFestival::CompletedIceSculptureAI>(npcAt(833520)).onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_EQ(sent(), exactly({}));
}

class NurseProbe final : public ev::CodeRedNurseAI {
public:
	using CodeRedNurseAI::CodeRedNurseAI;
	using CodeRedNurseAI::handleDialogStart;
};

class SculptureProbe final : public ev::iceFestival::CompletedIceSculptureAI {
public:
	using CompletedIceSculptureAI::CompletedIceSculptureAI;
	using CompletedIceSculptureAI::handleDialogStart;
};

/** CodeRedNurseAI.java:22-38: another npc opens page 1011; CompletedIceSculptureAI.java:22-26: its own page 1011 */
TEST_F(EventsA1AiTest, TheirOwnFirstPages) {
	Npc& other = npcAt(900100);
	install<NurseProbe>(other).handleDialogStart(player());
	Npc& sculpture = npcAt(833520);
	install<SculptureProbe>(sculpture).handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_DIALOG_WINDOW(other.getObjectId(), 1011)),
						  serializedFor(SM_DIALOG_WINDOW(sculpture.getObjectId(), 1011))}));
}

class SnakeProbe final : public ev::RainbowSnakeAI {
public:
	using RainbowSnakeAI::RainbowSnakeAI;
	using RainbowSnakeAI::handleCreatureNotSee;
};

/** RainbowSnakeAI.java:21-33: a player coming into sight is called once, again after leaving it */
TEST_F(EventsA1AiTest, TheRainbowSnakeCallsEachPlayerOnce) {
	SnakeProbe& ai = install<SnakeProbe>(npcAt(832963));
	ai.handleCreatureDetected(player());
	const size_t first = sent().size();
	EXPECT_GT(first, 0u) << "1501203: Hey, you there...";
	ai.handleCreatureDetected(player());
	EXPECT_EQ(sent().size(), first) << "once";
	ai.handleCreatureNotSee(player());
	ai.handleCreatureDetected(player());
	EXPECT_EQ(sent().size(), 2 * first) << "again after leaving the sight";
}

/** HalloweenPumpkinAI.java:34-42: a pumpkin never thinks and has 10 hp */
TEST_F(EventsA1AiTest, APumpkinHasTenHp) {
	auto& ai = install<ev::HalloweenPumpkinAI>(npcAt(219484));
	EXPECT_FALSE(ai.canThink());
	model::stats::calc::AdditionStat maxHp(model::stats::container::StatEnum::MAXHP, 1000, ai.getOwner());
	ai.modifyOwnerStat(maxHp);
	EXPECT_EQ(maxHp.getBase(), 10);
	model::stats::calc::AdditionStat speed(model::stats::container::StatEnum::SPEED, 6, ai.getOwner());
	ai.modifyOwnerStat(speed);
	EXPECT_EQ(speed.getBase(), 6) << "only MAXHP";
}

/** IceChunkAI.java:88-92: a chunk never respawns */
TEST_F(EventsA1AiTest, AnIceChunkNeverRespawns) {
	auto& ai = install<ev::iceFestival::IceChunkAI>(npcAt(833516));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_EQ(ai.ask(AIQuestion::ALLOW_DECAY), ai.NpcAI::ask(AIQuestion::ALLOW_DECAY));
}

class FakeCakeProbe final : public ev::FakeCakeAI {
public:
	using FakeCakeAI::FakeCakeAI;
	using FakeCakeAI::handleUseItemFinish;
};

/** FakeCakeAI.java:21-29: the fake cake dies on its use */
TEST_F(EventsA1AiTest, TheFakeCakeDies) {
	Npc& cake = npcAt(900100);
	install<FakeCakeProbe>(cake).handleUseItemFinish(player());
	EXPECT_TRUE(cake.isDead());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items::a1
