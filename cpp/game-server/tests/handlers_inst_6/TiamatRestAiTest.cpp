// I6-5b (docs/design/p6-i6-plan.md §3): the other AIs of ai/instance/tiamatStrongHold (the generals' helpers, Anuhart, Kumbanda, Shabokan,
// Kahrun, Surama, Muragan's men, the scientists, the statue, the eyes) on a real Tiamat Stronghold instance. Left to a gate: the casts and their
// effects (the rings', quakes', altars' and slowed time's debuffs in their bands, the final buffs), the walks, Surama's door.

#include "I6TestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/AdjutantAnuhartAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BladeStormAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/CapturedDrakanScientistAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/ChantraRingsAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/DistortedSpaceAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/EarthQuakeAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/ElectrocuteAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/FireStormAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/GravityAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/InvincibleShabokanAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/KharunAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/MuraganAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/SinkingSandAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/SlowTimeAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/SuramaTheTraitorAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TahabataAltarAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TahabataAltarFinalAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TiamatEyeAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TimeAcceleratorAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TraitorKumbandaAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/VershakaStatueAI.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"

#define AION_TIAMAT_REST(X)                                                                                                                   \
	X(AdjutantAnuhartAI)                                                                                                                      \
	X(BladeStormAI)                                                                                                                           \
	X(CapturedDrakanScientistAI)                                                                                                              \
	X(ChantraRingsAI)                                                                                                                         \
	X(DistortedSpaceAI)                                                                                                                       \
	X(EarthQuakeAI)                                                                                                                           \
	X(ElectrocuteAI)                                                                                                                          \
	X(FireStormAI)                                                                                                                            \
	X(GravityAI)                                                                                                                              \
	X(InvincibleShabokanAI)                                                                                                                   \
	X(KharunAI)                                                                                                                               \
	X(MuraganAI)                                                                                                                              \
	X(SinkingSandAI)                                                                                                                          \
	X(SlowTimeAI)                                                                                                                             \
	X(SuramaTheTraitorAI)                                                                                                                     \
	X(TahabataAltarAI)                                                                                                                        \
	X(TahabataAltarFinalAI)                                                                                                                   \
	X(TiamatEyeAI)                                                                                                                            \
	X(TimeAcceleratorAI)                                                                                                                      \
	X(TraitorKumbandaAI)                                                                                                                      \
	X(VershakaStatueAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {                                                                    \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_TIAMAT_REST(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ts = ::aion::gameserver::handlers::ai::instance::tiamatStrongHold;
using gameserver::ai::poll::AIQuestion;
using gameserver::ai::AIState;
using model::CreatureType;
using model::Race;
using questEngine::model::QuestState;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using skillengine::effect::AbnormalState;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleBackHome;
	using AI::handleCreatureMoved;
	using AI::handleCreatureSee;
	using AI::handleDialogStart;
	using AI::handleDied;
	using AI::handleSpawned;
};

class TiamatRestAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		tiamat = &create(TIAMAT_STRONGHOLD, TiamatStrongHoldInstance_instanceFactory);
		enter(*tiamat, 1030.0f, 300.0f, 410.0f);
		publishSkills({300, 20717, 20718, 20720, 20723, 20726, 20727, 20728, 20734, 20735, 20738, 20740, 20742, 20747, 20748, 20749, 20753, 20757,
			20759, 20938, 20939, 20940, 20941, 20942, 20943, 20952, 20970, 20971, 20972});
	}

	template <class AI>
	Probe<AI>& install(Npc& npc) {
		auto ai = std::make_unique<Probe<AI>>(npc);
		Probe<AI>& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 1032.0f, float y = 300.0f, float z = 410.0f) {
		return spawnNpc(*tiamat, npcId, x, y, z, int8_t{0});
	}

	size_t count(int32_t id) { return npcs(*tiamat, id).size(); }

	void aggro(Npc& npc) {
		InstanceKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
	}

	/** how often the npc said msgId (PacketSendUtility.broadcastMessage) */
	int64_t said(Npc& npc, int32_t msgId) {
		return countSent(serialized(SM_SYSTEM_MESSAGE(model::ChatType::NPC, runtime::Ptr<VisibleObject>(npc), msgId, std::vector<std::string>{})));
	}

	/** handleSpawned, then the npc stands until ms and is gone at ms */
	template <class AI>
	void lasts(int32_t npcId, int64_t ms) {
		runtime::Ptr<Npc> npc = place(npcId);
		install<AI>(*npc).handleSpawned();
		executor().advance(std::chrono::milliseconds(ms - 1));
		EXPECT_EQ(count(npcId), 1u) << npcId << " still stands";
		executor().advance(std::chrono::milliseconds(1));
		EXPECT_EQ(count(npcId), 0u) << npcId << " gone";
	}

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="tiamat" nameId="1" stack="TIAMAT" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* tiamat = nullptr;
};

/** The 21 AION_AI factories build their classes */
TEST_F(TiamatRestAiTest, TheFactories) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = ts::cls##_aiFactory(*npc);                                                                                                \
		EXPECT_NE(dynamic_cast<ts::cls*>(ai.get()), nullptr) << #cls;                                                                         \
	}
	AION_TIAMAT_REST(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** BladeStormAI, ElectrocuteAI, FireStormAI, GravityAI, ChantraRingsAI, EarthQuakeAI, TimeAcceleratorAI, SinkingSandAI, DistortedSpaceAI,
 * TiamatEyeAI: each goes at its time after its spawn; the last fire storm (283102) and the last altar stay */
TEST_F(TiamatRestAiTest, TheHelpersGoInTime) {
	lasts<ts::BladeStormAI>(283099, 10000);
	lasts<ts::ElectrocuteAI>(283051, 10500);
	lasts<ts::FireStormAI>(283045, 20000);
	lasts<ts::GravityAI>(283109, 20000);
	lasts<ts::ChantraRingsAI>(283171, 20000);
	lasts<ts::EarthQuakeAI>(283082, 9000);
	lasts<ts::TimeAcceleratorAI>(283086, 20000);
	lasts<ts::SinkingSandAI>(283083, 4000);
	lasts<ts::DistortedSpaceAI>(283097, 8000);
	lasts<ts::TiamatEyeAI>(283177, 5000);
	install<ts::FireStormAI>(*place(283102)).handleSpawned();
	install<ts::TahabataAltarFinalAI>(*place(283121)).handleSpawned();
	executor().advance(std::chrono::seconds(60));
	EXPECT_EQ(count(283102), 1u);
	EXPECT_EQ(count(283121), 1u);
}

/** The helpers' ask: no decay, respawn or rewards */
TEST_F(TiamatRestAiTest, TheHelpersLeaveNothing) {
	runtime::Ptr<Npc> npc = place(900200);
	std::vector<std::unique_ptr<gameserver::ai::AbstractAI>> ais;
	ais.push_back(std::make_unique<ts::BladeStormAI>(*npc));
	ais.push_back(std::make_unique<ts::ElectrocuteAI>(*npc));
	ais.push_back(std::make_unique<ts::FireStormAI>(*npc));
	ais.push_back(std::make_unique<ts::GravityAI>(*npc));
	ais.push_back(std::make_unique<ts::TahabataAltarFinalAI>(*npc));
	ais.push_back(std::make_unique<ts::ChantraRingsAI>(*npc));
	ais.push_back(std::make_unique<ts::EarthQuakeAI>(*npc));
	ais.push_back(std::make_unique<ts::TimeAcceleratorAI>(*npc));
	ais.push_back(std::make_unique<ts::SlowTimeAI>(*npc));
	ais.push_back(std::make_unique<ts::TahabataAltarAI>(*npc));
	ais.push_back(std::make_unique<ts::DistortedSpaceAI>(*npc));
	for (size_t i = 0; i < ais.size(); i++) {
		EXPECT_FALSE(ais[i]->ask(AIQuestion::ALLOW_DECAY)) << i;
		EXPECT_FALSE(ais[i]->ask(AIQuestion::ALLOW_RESPAWN)) << i;
		EXPECT_FALSE(ais[i]->ask(AIQuestion::REWARD_AP_XP_DP_LOOT)) << i;
	}
}

/** AdjutantAnuhartAI.java:26-65: Blade Storm (20747) makes him a sanctuary with a blade storm (283099) until it ends; another skill does not;
 * his HP phases' self buffs are left to a gate */
TEST_F(TiamatRestAiTest, AnuhartsBladeStorm) {
	runtime::Ptr<Npc> anuhart = place(219357);
	auto& ai = install<ts::AdjutantAnuhartAI>(*anuhart);
	const auto* bladeStorm = dataholders::DataManager::SKILL_DATA->getSkillTemplate(20747);
	const auto* other = dataholders::DataManager::SKILL_DATA->getSkillTemplate(20748);
	ai.onStartUseSkill(other, 1);
	EXPECT_EQ(count(283099), 0u);
	EXPECT_FALSE(anuhart->getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY));
	ai.onStartUseSkill(bladeStorm, 1);
	EXPECT_EQ(count(283099), 1u);
	EXPECT_TRUE(anuhart->getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY));
	ai.onEndUseSkill(other, 1);
	EXPECT_TRUE(anuhart->getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY));
	ai.onEndUseSkill(bladeStorm, 1);
	EXPECT_FALSE(anuhart->getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY));
}

/** VershakaStatueAI.java:23-35: dialog 1011; any choice closes it */
TEST_F(TiamatRestAiTest, VershakasStatue) {
	runtime::Ptr<Npc> statue = place(730668);
	auto& ai = install<ts::VershakaStatueAI>(*statue);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(statue->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(statue->getObjectId(), 0))), 1);
}

/** KharunAI.java:25-56: the first choice takes Kahrun; 3 s later his fighting self (800335) breaks the door (730612) and the field (730613) */
TEST_F(TiamatRestAiTest, KahrunBreaksTheDoor) {
	if (count(730613) == 0)
		place(730613, 1040.0f);
	if (count(730612) == 0)
		place(730612, 1045.0f);
	const size_t fields = count(730613);
	runtime::Ptr<Npc> door = tiamat->getNpc(730612);
	runtime::Ptr<Npc> kharun = place(800456);
	auto& ai = install<ts::KharunAI>(*kharun);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(kharun->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_EQ(count(800456), 1u) << "another choice";
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(kharun->getObjectId(), 0))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(count(800456), 0u);
	executor().advance(std::chrono::milliseconds(2999));
	EXPECT_EQ(count(800335), 0u);
	EXPECT_FALSE(door->isDead());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(800335), 1u);
	EXPECT_TRUE(door->isDead());
	EXPECT_EQ(count(730613), fields - 1);
}

/** TiamatEyeAI.java:20-51: the 4.0 eye (283913) speaks 2 s after its spawn; the eyes the handler spawns (283177) say nothing (Java bug J-I6-6) */
TEST_F(TiamatRestAiTest, TheEyesSpeakOnlyWithTheirOldIds) {
	runtime::Ptr<Npc> old = place(283913);
	runtime::Ptr<Npc> eye = place(283177);
	InstanceKnownListPairing::pair(*old, player());
	InstanceKnownListPairing::pair(*eye, player());
	install<ts::TiamatEyeAI>(*old).handleSpawned();
	install<ts::TiamatEyeAI>(*eye).handleSpawned();
	executor().advance(std::chrono::milliseconds(1999));
	EXPECT_EQ(said(*old, 1500679), 0);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(said(*old, 1500679), 1);
	EXPECT_EQ(said(*eye, 1500679), 0) << "J-I6-6";
	EXPECT_EQ(said(*eye, 1500680) + said(*eye, 1500681) + said(*eye, 1500682), 0) << "J-I6-6";
}

/** TahabataAltarAI.java:32-47 with the correction of 2026-10-19 (was J-I6-5): 283116 casts 20970 at a player 25-37 m away, 283118 20971 at
 * 20-25 m; nearer or farther, or an npc, nothing (Java checked the bands on 283253/283255: no altar cast) */
TEST_F(TiamatRestAiTest, TahabatasAltarsDebuffInTheirBands) {
	auto casts = [&](int32_t altarId, float x, Creature& creature) {
		runtime::Ptr<Npc> altar = place(altarId, x);
		auto& ai = install<ts::TahabataAltarAI>(*altar);
		ai.handleCreatureMoved(creature);
		return altar->getGameStats()->getLastSkillTime() != 0;
	};
	EXPECT_TRUE(casts(283116, 1060.0f, player())) << "283116 at 30 m";
	EXPECT_FALSE(casts(283116, 1050.0f, player())) << "283116 at 20 m";
	EXPECT_FALSE(casts(283116, 1070.0f, player())) << "283116 at 40 m";
	EXPECT_TRUE(casts(283118, 1052.0f, player())) << "283118 at 22 m";
	EXPECT_FALSE(casts(283118, 1058.0f, player())) << "283118 at 28 m";
	runtime::Ptr<Npc> npc = place(900200, 1030.0f);
	EXPECT_FALSE(casts(283116, 1060.0f, *npc)) << "an npc";
}

/** SuramaTheTraitorAI.java:26-56: 10 s after her spawn the talk; 8 s later Raksha (219356) turns on her and becomes attackable; her death speaks */
TEST_F(TiamatRestAiTest, SuramasBetrayal) {
	runtime::Ptr<Npc> raksha = place(219356, 1040.0f);
	runtime::Ptr<Npc> surama = place(800433);
	InstanceKnownListPairing::pair(*raksha, player());
	InstanceKnownListPairing::pair(*surama, player());
	auto& ai = install<ts::SuramaTheTraitorAI>(*surama);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(said(*surama, 390841), 0);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(said(*surama, 390841), 1);
	executor().advance(std::chrono::seconds(6));
	EXPECT_EQ(said(*raksha, 390843), 1);
	raksha->overrideNpcType(CreatureType::PEACE);
	executor().advance(std::chrono::milliseconds(1999));
	EXPECT_EQ(raksha->getType(player()), CreatureType::PEACE);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(raksha->getType(player()), CreatureType::ATTACKABLE);
	ai.handleDied();
	executor().advance(std::chrono::seconds(2));
	EXPECT_EQ(said(*surama, 390845), 1);
}

/** MuraganAI.java:31-87: 800438 speaks at its spawn and stays; a player within 15 m sets 800436 off (it speaks and goes) and 800435 off once (it
 * turns the guard captains into 283145 and is deleted 10 s later); farther or an npc, nothing */
TEST_F(TiamatRestAiTest, MuragansMen) {
	runtime::Ptr<Npc> watcher = place(800438);
	InstanceKnownListPairing::pair(*watcher, player());
	auto& watcherAi = install<ts::MuraganAI>(*watcher);
	watcherAi.handleSpawned();
	executor().advance(std::chrono::seconds(1));
	EXPECT_EQ(said(*watcher, 390852), 1);
	watcherAi.handleCreatureSee(player());
	EXPECT_EQ(count(800438), 1u);

	runtime::Ptr<Npc> doorman = place(800436);
	InstanceKnownListPairing::pair(*doorman, player());
	install<ts::MuraganAI>(*doorman).handleCreatureMoved(player());
	EXPECT_EQ(said(*doorman, 390835), 1);
	EXPECT_EQ(count(800436), 0u);

	place(219392, 1050.0f);
	place(219392, 1055.0f);
	runtime::Ptr<Npc> far = place(800435, 1050.0f);
	install<ts::MuraganAI>(*far).handleCreatureMoved(player());
	EXPECT_EQ(count(219392), 2u) << "20 m away";
	runtime::Ptr<Npc> muragan = place(800435);
	auto& ai = install<ts::MuraganAI>(*muragan);
	ai.handleCreatureSee(*place(900200, 1031.0f));
	EXPECT_EQ(count(219392), 2u) << "an npc";
	ai.handleCreatureSee(player());
	EXPECT_EQ(count(219392), 0u);
	EXPECT_EQ(count(283145), 2u);
	place(219392, 1050.0f);
	ai.handleCreatureMoved(player());
	EXPECT_EQ(count(219392), 1u) << "once";
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count(800435), 2u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(800435), 1u) << "he goes, the far one stays";
	EXPECT_EQ(count(800438), 1u) << "800438 does not walk";
}

/** CapturedDrakanScientistAI.java:38-84: the first player it sees makes it watch the eyes (219390) within 25 m; two of them dead, it walks and
 * 1-2 s + 9 s later moves the quest and goes */
TEST_F(TiamatRestAiTest, TheScientistEscapes) {
	const int32_t questId = player().getRace() == Race::ELYOS ? 30708 : 30758;
	runtime::Ref<QuestState> qs = QuestState::create(questId, questEngine::model::QuestStatus::START);
	player().getQuestStateList()->addQuest(questId, *qs);
	runtime::Ptr<Npc> scientist = place(800425);
	runtime::Ptr<Npc> eye1 = place(219390, 1042.0f);
	runtime::Ptr<Npc> eye2 = place(219390, 1032.0f, 310.0f);
	runtime::Ptr<Npc> farEye = place(219390, 1032.0f, 340.0f);
	for (Npc* npc : {eye1.get(), eye2.get(), farEye.get()})
		InstanceKnownListPairing::pair(*scientist, *npc);
	InstanceKnownListPairing::pair(*scientist, player());
	auto& ai = install<ts::CapturedDrakanScientistAI>(*scientist);
	ai.handleCreatureSee(*eye1);
	ai.handleCreatureSee(player());
	ai.handleCreatureSee(player());
	farEye->getObserveController()->notifyDeathObservers(player());
	eye1->getObserveController()->notifyDeathObservers(player());
	executor().advance(std::chrono::seconds(15));
	EXPECT_EQ(count(800425), 1u) << "one eye down";
	EXPECT_NE(ai.getState(), AIState::WALKING);
	eye2->getObserveController()->notifyDeathObservers(player());
	int32_t waited = 0;
	while (ai.getState() != AIState::WALKING && waited < 3000) {
		executor().advance(std::chrono::milliseconds(100));
		waited += 100;
	}
	EXPECT_GE(waited, 1000);
	EXPECT_LE(waited, 2000);
	executor().advance(std::chrono::milliseconds(8999));
	EXPECT_EQ(count(800425), 1u);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(800425), 0u);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs))), 1);
}

/** CapturedDrakanScientistAI.java:72-84: a quest at step 5 stays */
TEST_F(TiamatRestAiTest, TheScientistsQuestStopsAtFive) {
	const int32_t questId = player().getRace() == Race::ELYOS ? 30708 : 30758;
	runtime::Ref<QuestState> qs = QuestState::create(questId, questEngine::model::QuestStatus::START);
	qs->setQuestVar(5);
	player().getQuestStateList()->addQuest(questId, *qs);
	runtime::Ptr<Npc> scientist = place(800425);
	runtime::Ptr<Npc> eye1 = place(219390, 1042.0f);
	runtime::Ptr<Npc> eye2 = place(219390, 1032.0f, 310.0f);
	InstanceKnownListPairing::pair(*scientist, *eye1);
	InstanceKnownListPairing::pair(*scientist, *eye2);
	install<ts::CapturedDrakanScientistAI>(*scientist).handleCreatureSee(player());
	eye1->getObserveController()->notifyDeathObservers(player());
	eye2->getObserveController()->notifyDeathObservers(player());
	executor().advance(std::chrono::seconds(12));
	EXPECT_EQ(count(800425), 0u);
	EXPECT_EQ(qs->getQuestVarById(0), 5);
}

/** InvincibleShabokanAI.java:32-107: from the first attack every 30 s from 5 s an earthquake (one 283082 at a time) or a sinking (283083 and
 * 283084 under the player within 30 m); his death stops them */
TEST_F(TiamatRestAiTest, ShabokansQuakesAndSands) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	runtime::Ptr<Npc> shabokan = place(219352);
	auto& ai = install<ts::InvincibleShabokanAI>(*shabokan);
	aggro(*shabokan);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	executor().advance(std::chrono::milliseconds(4999));
	EXPECT_EQ(count(283082) + count(283083), 0u);
	executor().advance(std::chrono::milliseconds(1));
	bool quake = false;
	size_t sinks = 0;
	for (int32_t cycle = 0; cycle < 16; cycle++) {
		if (cycle > 0)
			executor().advance(std::chrono::seconds(30));
		const size_t sands = count(283083);
		EXPECT_LE(count(283082), 1u) << "one quake at a time";
		EXPECT_EQ(count(283084), sands);
		quake = quake || count(283082) == 1;
		ASSERT_GE(sands, sinks);
		EXPECT_LE(sands, sinks + 1) << "one event a cycle";
		sinks = sands;
	}
	EXPECT_TRUE(quake);
	EXPECT_GT(sinks, 1u);
	for (const runtime::Ptr<Npc>& sand : npcs(*tiamat, 283083))
		EXPECT_LE(std::hypot(sand->getX() - 1030.0f, sand->getY() - 300.0f), 0.01f) << "under the player";
	ai.handleDied();
	executor().advance(std::chrono::seconds(120));
	EXPECT_EQ(count(283083), sinks) << "stopped";
}

/** TraitorKumbandaAI.java:28-80: an attack now and then calls seven time accelerators (one on him, six 10-20 m around) while none stands, and at
 * 50 % HP or less his ghost (283085) while none stands; his death takes the accelerators and the slowed time (283088) */
TEST_F(TiamatRestAiTest, KumbandasAccelerators) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	runtime::Ptr<Npc> kumbanda = place(219355);
	auto& ai = install<ts::TraitorKumbandaAI>(*kumbanda);
	aggro(*kumbanda);
	kumbanda->getLifeStats()->setCurrentHpPercent(60);
	int32_t attacks = 0;
	while (count(283086) == 0 && attacks < 1000) {
		ai.handleAttack(runtime::Ptr<Creature>(player()));
		attacks++;
	}
	ASSERT_EQ(count(283086), 7u);
	EXPECT_GT(attacks, 1);
	int32_t onHim = 0;
	for (const runtime::Ptr<Npc>& accelerator : npcs(*tiamat, 283086)) {
		const float d = std::hypot(accelerator->getX() - 1032.0f, accelerator->getY() - 300.0f);
		if (d < 0.01f)
			onHim++;
		else {
			EXPECT_GE(d, 9.99f);
			EXPECT_LE(d, 20.01f);
		}
	}
	EXPECT_EQ(onHim, 1);
	EXPECT_EQ(count(283085), 0u) << "above 50 %";
	kumbanda->getLifeStats()->setCurrentHpPercent(40);
	for (int32_t attack = 0; attack < 300; attack++)
		ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(283085), 1u) << "his ghost, once";
	EXPECT_EQ(count(283086), 7u) << "not while they stand";
	place(283088, 1040.0f);
	ai.handleDied();
	EXPECT_EQ(count(283086), 0u);
	EXPECT_EQ(count(283088), 0u);
	EXPECT_EQ(count(283085), 1u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
