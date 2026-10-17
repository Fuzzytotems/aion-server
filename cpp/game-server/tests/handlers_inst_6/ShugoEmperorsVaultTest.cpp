// I6-3 (docs/design/p6-i6-plan.md §3): TheShugoEmperorsVault (game-server/data/handlers/instance) and the theShugoEmperorsVault AIs
// (game-server/data/handlers/ai/instance/theShugoEmperorsVault) on a real Vault instance. Left to a gate: the score packet's bytes (its time
// reads the clock), the doors (no door npcs here), the morph effects' cache (onStartEffect needs a real skill's Effect), the revive and the
// teleports (HousingService), the key rewards (item rows) and the casts' effects.

#include "I6TestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Bosses.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_HealTower.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_In_Npc.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Shugos.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/RuthlessJabaraki.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/ShugoMorpher.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/VaultKeeper.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/WatchmanHokuruki.h"
#include "aion/gameserver/handlers/instance/TheShugoEmperorsVault.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/instance/instancescore/NormalScore.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/ChatUtil.h"

#define AION_VAULT_AIS(X)                                                                                                                     \
	X(IDSweep_Bosses)                                                                                                                         \
	X(IDSweep_HealTower)                                                                                                                      \
	X(IDSweep_In_Npc)                                                                                                                         \
	X(IDSweep_Shugos)                                                                                                                         \
	X(RuthlessJabaraki)                                                                                                                       \
	X(ShugoMorpher)                                                                                                                           \
	X(VaultKeeper)                                                                                                                            \
	X(WatchmanHokuruki)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {                                                               \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_VAULT_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace sv = ::aion::gameserver::handlers::ai::instance::theShugoEmperorsVault;
using gameserver::ai::AIState;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::instance::instancescore::NormalScore;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleBackHome;
	using AI::handleDialogStart;
	using AI::handleDied;
	using AI::handleSpawned;
};

class ShugoEmperorsVaultTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		vault = &create(SHUGO_EMPERORS_VAULT, TheShugoEmperorsVault_instanceFactory);
		enter(*vault, 545.0f, 310.0f, 400.4f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 550.0f, float y = 390.0f, float z = 395.5f) {
		return spawnNpc(*vault, npcId, x, y, z, int8_t{0});
	}

	size_t count(int32_t id) { return npcs(*vault, id).size(); }

	NormalScore& score() { return dynamic_cast<NormalScore&>(*vault->getInstanceHandler()->getInstanceScore()); }

	void dies(int32_t npcId) { vault->getInstanceHandler()->onDie(*place(npcId)); }

	void start() { vault->getInstanceHandler()->onOpenDoor(430); }

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="vault" nameId="1" stack="VAULT" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* vault = nullptr;
};

/** The eight AION_AI factories build their classes */
TEST_F(ShugoEmperorsVaultTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = sv::cls##_aiFactory(*npc);                                                                                                \
		EXPECT_NE(dynamic_cast<sv::cls*>(ai.get()), nullptr) << #cls;                                                                         \
	}
	AION_VAULT_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** TheShugoEmperorsVault.java:54-72, 105-108: the morph Shugos wait; 60 s later the run starts with the Shugos and the first room */
TEST_F(ShugoEmperorsVaultTest, TheRunStartsAfterAMinute) {
	EXPECT_EQ(count(833491) + count(833492) + count(833493), 3u);
	EXPECT_TRUE(score().isPreparing());
	executor().advance(std::chrono::milliseconds(59999));
	EXPECT_EQ(count(235660), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(score().isStartProgress());
	EXPECT_EQ(count(832927), 3u);
	EXPECT_EQ(count(235629), 3u);
	EXPECT_EQ(count(235630), 2u);
	EXPECT_EQ(count(235660), 1u) << "Ruthless Jabaraki";
}

/** TheShugoEmperorsVault.java:74-86: door 430 starts the run at once, and the minute's timer does nothing more */
TEST_F(ShugoEmperorsVaultTest, Door430StartsTheRunAtOnce) {
	start();
	EXPECT_TRUE(score().isStartProgress());
	EXPECT_EQ(count(235660), 1u);
	executor().advance(std::chrono::minutes(1));
	EXPECT_EQ(count(235660), 1u) << "started once";
	vault->getInstanceHandler()->onOpenDoor(431);
	EXPECT_EQ(count(235660), 1u);
}

/** TheShugoEmperorsVault.java:230-349: kills score only while the run is on, each told with the npc's name */
TEST_F(ShugoEmperorsVaultTest, KillsScore) {
	dies(235629);
	EXPECT_EQ(score().getPoints(), 0) << "not started";
	start();
	runtime::Ptr<Npc> skirmisher = place(235629);
	vault->getInstanceHandler()->onDie(*skirmisher);
	EXPECT_EQ(score().getPoints(), 180);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(skirmisher->getObjectTemplate()->getL10n(), 180))), 1);
	dies(235651);
	dies(235683);
	EXPECT_EQ(score().getPoints(), 180 + 1400 + 88000);
}

/** TheShugoEmperorsVault.java:238-261: Jabaraki opens the second room, two Turncoats Hokuruki's, Hokuruki the vault and the fourth room */
TEST_F(ShugoEmperorsVaultTest, TheRoomsFollowTheBosses) {
	start();
	dies(235660);
	EXPECT_EQ(count(235652), 15u);
	EXPECT_EQ(count(235653), 4u);
	EXPECT_EQ(count(235644), 1u);
	dies(235641);
	EXPECT_EQ(count(235634), 0u);
	dies(235641);
	EXPECT_EQ(count(235634), 1u) << "Watchman Hokuruki";
	dies(235634);
	EXPECT_EQ(count(832925), 1u) << "the vault door";
	EXPECT_EQ(count(832918), 2u) << "the two healing springs";
	EXPECT_EQ(count(235681), 13u) << "the fourth room";
	executor().advance(std::chrono::minutes(1));
	EXPECT_EQ(count(832918), 1u) << "the inner spring heals for a minute";
}

/** TheShugoEmperorsVault.java:308-342: after Hokuruki the points call the bosses in turn, each once */
TEST_F(ShugoEmperorsVaultTest, ThePointsCallTheBosses) {
	start();
	score().setPoints(49000);
	dies(235629);
	EXPECT_EQ(count(235640), 0u) << "not the last stage yet";
	dies(235634); // Hokuruki: 51,220 points and the last stage
	EXPECT_EQ(count(235640), 1u) << "Captain Mirez at 50,000";
	dies(235629);
	EXPECT_EQ(count(235640), 1u) << "once";
	dies(235640); // + 12,000, stage 1
	EXPECT_EQ(count(235685), 0u) << "63,400 < 82,500";
	score().setPoints(82500);
	dies(235629);
	EXPECT_EQ(count(235685), 1u) << "Longknife Zodica";
}

/** TheShugoEmperorsVault.java:97-103, 409-445: 8 minutes end the run; with fewer than 180 points rank 8, no butler */
TEST_F(ShugoEmperorsVaultTest, TheTimeEndsTheRun) {
	start();
	executor().advance(std::chrono::milliseconds(480499));
	EXPECT_FALSE(score().isRewarded());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(score().isRewarded());
	EXPECT_EQ(score().getRank(), 8);
	EXPECT_EQ(count(832932), 0u);
	EXPECT_EQ(count(235660), 0u) << "everything despawned";
}

/** TheShugoEmperorsVault.java:301-305, 409-445: Gradi's death ends the run; 180-52,099 points give rank 5, the butler and the chests */
TEST_F(ShugoEmperorsVaultTest, GradisDeathEndsTheRun) {
	start();
	score().setPoints(-224000 + 1000); // 1,000 points after Gradi's 224,000
	dies(235647);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_IDSweep_Stage2_End())), 1);
	EXPECT_EQ(score().getRank(), 5);
	EXPECT_EQ(count(832932), 1u) << "the butler";
	EXPECT_GT(count(832930), 0u) << "the chests";
	EXPECT_EQ(count(832919), 0u);
}

/** IDSweep_Shugos.java:26-48, IDSweep_Bosses.java:17-23: an intruder spawned after the run is deleted; auto attacks use the template attack */
TEST_F(ShugoEmperorsVaultTest, TheIntruders) {
	runtime::Ptr<Npc> intruder = place(235629);
	auto& ai = install<Probe<sv::IDSweep_Shugos>>(*intruder);
	ai.handleSpawned();
	EXPECT_TRUE(intruder->isSpawned());
	const float attack = static_cast<float>(intruder->getGameStats()->getStatsTemplate()->getAttack());
	ASSERT_GT(attack, 100.0f);
	for (int32_t hit = 0; hit < 20; hit++) { // the template attack -20 % to +25 %
		float damage = ai.modifyOwnerDamage(100.0f, player(), nullptr);
		EXPECT_GE(damage, std::floor(attack * 0.8f));
		EXPECT_LE(damage, std::ceil(attack * 1.25f));
	}
	runtime::Ptr<Npc> boss = place(235660);
	auto& bossAi = install<Probe<sv::IDSweep_Bosses>>(*boss);
	EXPECT_TRUE(bossAi.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	start();
	executor().advance(std::chrono::milliseconds(480500)); // the run's end
	runtime::Ptr<Npc> late = place(235629);
	auto& lateAi = install<Probe<sv::IDSweep_Shugos>>(*late);
	lateAi.handleSpawned();
	EXPECT_FALSE(late->isSpawned());
}

/** RuthlessJabaraki.java:31-128: of his phases only 80 % (four Peons) and 35 % (twelve Scouts) call adds (Java bug J-I6-2); his death deletes
 * him */
TEST_F(ShugoEmperorsVaultTest, JabarakisWaves) {
	runtime::Ptr<Npc> jabaraki = place(235660, 548.0f, 312.0f, 400.4f); // near the player: a far target sends him home
	auto& ai = install<Probe<sv::RuthlessJabaraki>>(*jabaraki);
	InstanceKnownListPairing::pair(*jabaraki, player());
	jabaraki->getAggroList().addDamage(player(), 1, true, std::nullopt);
	jabaraki->getLifeStats()->setCurrentHpPercent(95);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(235631), 0u) << "96 % calls nothing";
	jabaraki->getLifeStats()->setCurrentHpPercent(79);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(235631), 4u);
	jabaraki->getLifeStats()->setCurrentHpPercent(34);
	for (int32_t phase = 0; phase < 4; phase++)
		ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(235630), 12u);
	EXPECT_EQ(count(235629), 0u);
	ai.handleDied();
	EXPECT_FALSE(jabaraki->isSpawned());
}

/** WatchmanHokuruki.java:33-82: at 100 % four 235632 3-5 m around him; at 75 % a marksman and two snipers at one of the three places */
TEST_F(ShugoEmperorsVaultTest, HokurukisAdds) {
	runtime::Ptr<Npc> hokuruki = place(235634, 548.0f, 312.0f, 400.4f); // near the player: a far target sends him home
	auto& ai = install<Probe<sv::WatchmanHokuruki>>(*hokuruki);
	InstanceKnownListPairing::pair(*hokuruki, player());
	hokuruki->getAggroList().addDamage(player(), 1, true, std::nullopt);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	ASSERT_EQ(count(235632), 4u);
	for (const runtime::Ptr<Npc>& add : npcs(*vault, 235632)) {
		double distance = std::hypot(add->getX() - 548.0f, add->getY() - 312.0f);
		EXPECT_GE(distance, 2.99);
		EXPECT_LE(distance, 5.01);
	}
	hokuruki->getLifeStats()->setCurrentHpPercent(74);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(236083), 1u);
	EXPECT_EQ(count(235649), 2u);
	ai.handleDied();
	EXPECT_FALSE(hokuruki->isSpawned());
}

/** IDSweep_HealTower.java:24-55: every 3 s from 2 s it heals (targets itself) only while a hurt living player stands within 3 m */
TEST_F(ShugoEmperorsVaultTest, TheSpringHealsTheHurtNearby) {
	publishSkills({21837});
	runtime::Ptr<Npc> spring = place(832918, 546.0f, 310.0f, 400.4f);
	auto& ai = install<Probe<sv::IDSweep_HealTower>>(*spring);
	InstanceKnownListPairing::pair(*spring, player());
	ai.handleSpawned();
	executor().advance(std::chrono::seconds(2));
	EXPECT_EQ(spring->getTarget(), nullptr) << "the player is not hurt";
	player().getLifeStats()->setCurrentHpPercent(50);
	executor().advance(std::chrono::seconds(3));
	EXPECT_EQ(spring->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(spring));
	ai.handleDied();
}

/** IDSweep_In_Npc.java:27-39, VaultKeeper.java:31-61: the guide's dialog 1011; the keeper ignores other choices */
TEST_F(ShugoEmperorsVaultTest, TheGuideAndTheKeeper) {
	runtime::Ptr<Npc> guide = place(900200, 546.0f, 310.0f, 400.4f);
	auto& ai = install<Probe<sv::IDSweep_In_Npc>>(*guide);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(guide->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	runtime::Ptr<Npc> keeper = place(900300, 546.0f, 311.0f, 400.4f);
	auto& keeperAi = install<sv::VaultKeeper>(*keeper);
	EXPECT_TRUE(keeperAi.onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(keeper->getObjectId(), 0))), 0);
}

/** ShugoMorpher.java:36-101: attacks do nothing and no damage; no loot or rewards */
TEST_F(ShugoEmperorsVaultTest, TheMorphShugos) {
	runtime::Ptr<Npc> shugo = npcs(*vault, 833491)[0];
	auto& ai = install<Probe<sv::ShugoMorpher>>(*shugo);
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 500.0f, nullptr), 0.0f);
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_TRUE(shugo->isSpawned());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
