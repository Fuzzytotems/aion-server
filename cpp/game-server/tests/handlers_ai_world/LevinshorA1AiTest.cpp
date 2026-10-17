// A1, lane C, slice 5 (worlds/levinshor): VocolithAI and AncientMonsterAI (game-server/data/handlers/ai/worlds/levinshor) on npcs of a real
// Levinshor map instance, each AI installed with replaceAi. The ancient monster's leash (a move arrived 15 m from its spawn) needs the move
// controller and is left to a retest, as are the use bar's abort by a move and the 21780 damage.

#include "ZoneA1TestSupport.h"

#include <chrono>
#include <cstdint>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/handlers/ai/worlds/levinshor/AncientMonsterAI.h"
#include "aion/gameserver/handlers/ai/worlds/levinshor/VocolithAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"

#define AION_LEVINSHOR_AIS(X)                                                                                                                 \
	X(AncientMonsterAI)                                                                                                                       \
	X(VocolithAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::worlds::levinshor {                                                                             \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_LEVINSHOR_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::ai::worlds::test {
namespace {

namespace lev = ::aion::gameserver::handlers::ai::worlds::levinshor;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::aion::serverpackets::SM_USE_OBJECT;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleDialogStart;
	using AI::handleSpawned;
};

class LevinshorA1AiTest : public ZoneA1AiTest {
protected:
	void SetUp() override {
		ZoneA1AiTest::SetUp();
		open(LEVINSHOR, 1000.0f, 1000.0f, 200.0f);
	}

	size_t bosses() { return count(235217) + count(235218) + count(235219) + count(235220); }
};

/** The two AION_AI factories build their classes */
TEST_F(LevinshorA1AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200, 1002.0f, 1000.0f, 200.0f);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = lev::cls##_aiFactory(*npc);                                                                                               \
		EXPECT_NE(dynamic_cast<lev::cls*>(ai.get()), nullptr) << #cls;                                                                        \
	}
	AION_LEVINSHOR_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** AncientMonsterAI.java:23-30, 39-45: deleted an hour after its spawn while alive; 1.5 times its damage */
TEST_F(LevinshorA1AiTest, AnAncientMonsterLeavesAfterAnHour) {
	runtime::Ptr<Npc> monster = place(235217, 1002.0f, 1000.0f, 200.0f);
	auto& ai = install<Probe<lev::AncientMonsterAI>>(*monster);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 150.0f);
	ai.handleSpawned();
	executor().advance(std::chrono::hours(1) - std::chrono::milliseconds(1));
	EXPECT_TRUE(monster->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(monster->isSpawned());
}

/** VocolithAI.java:38-62: a click starts a 1.5 s use bar that opens dialog 1011 */
TEST_F(LevinshorA1AiTest, TheVocolithsUseBarOpensItsDialog) {
	runtime::Ptr<Npc> vocolith = place(900200, 1002.0f, 1000.0f, 200.0f);
	auto& ai = install<Probe<lev::VocolithAI>>(*vocolith);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_USE_OBJECT(player().getObjectId(), vocolith->getObjectId(), 1500, 1))), 1);
	executor().advance(std::chrono::milliseconds(1499));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(vocolith->getObjectId(), 1011))), 0);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(countSent(serialized(SM_USE_OBJECT(player().getObjectId(), vocolith->getObjectId(), 1500, 2))), 1);
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(vocolith->getObjectId(), 1011))), 1);
}

/** VocolithAI.java:64-77: without the Ancestor's Relic the choice fails with dialog NO_RIGHT; any other choice closes the dialog */
TEST_F(LevinshorA1AiTest, TheVocolithWantsTheRelic) {
	runtime::Ptr<Npc> vocolith = place(900200, 1002.0f, 1000.0f, 200.0f);
	auto& ai = install<lev::VocolithAI>(*vocolith);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_FNAMED_FAIL())), 1);
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(vocolith->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 1);
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(vocolith->getObjectId(), 0))), 0);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(vocolith->getObjectId(), 0))), 1);
}

/** VocolithAI.java:79-98: the relic is used up, the Vocolith dies, and 5 s later one of the four ancient monsters stands where it stood */
TEST_F(LevinshorA1AiTest, TheVocolithCallsAnAncientMonster) {
	giveItem(185000216, 2);
	runtime::Ptr<Npc> vocolith = place(900200, 1002.0f, 1000.0f, 200.0f);
	auto& ai = install<lev::VocolithAI>(*vocolith);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(player().getInventory().getItemCountByItemId(185000216), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_LDF4_ADVANCE_FNAMED_SPAWN_ITEM())), 1);
	EXPECT_TRUE(vocolith->isDead());
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0)) << "used: the second choice takes nothing";
	EXPECT_EQ(player().getInventory().getItemCountByItemId(185000216), 1);
	executor().advance(std::chrono::milliseconds(4999));
	EXPECT_EQ(bosses(), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(bosses(), 1u);
	EXPECT_TRUE(vocolith->isSpawned()) << "its decay comes at 7.5 s";
	executor().advance(std::chrono::milliseconds(2500));
	EXPECT_FALSE(vocolith->isSpawned());
}

} // namespace
} // namespace aion::gameserver::handlers::ai::worlds::test
