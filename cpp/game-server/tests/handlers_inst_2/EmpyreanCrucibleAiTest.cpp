// P6-I2 Empyrean Crucible's AIs (game-server/data/handlers/ai/instance/empyreanCrucible) on npcs of a real Empyrean Crucible instance, each
// AI installed on its npc and driven through its protected events. The skills are plain templates (one per id, I5AiTest.cpp's); a probe
// records the skills its npc starts (AI.onStartUseSkill, which none of these AIs overrides). The test world sends an attacked npc home at once
// (no fight here): the probes go home only when a case asks (goHome). Left out: the draky's laps (no walker route
// in the test data), the arbiters' teleports and the record keepers whose stage teleports the players (START_STAGE_5 to 9).

#include "I2TestSupport.h"
#include "CrucibleTestSupport.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/ArminosDrakyAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/EmpyreanAdministratorArminosAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/EmpyreanArbiterAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/EmpyreanRecordKeeperAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/KingConsierdAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/MagePreceptorAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/PriestPreceptorAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/QueenAlukinaAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/RM1337AI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/SparkOfDarknessAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/SpectralWarriorAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/StrangeCreatureAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/TakunGojiraAI.h"
#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/WarriorPreceptorAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {
::aion::gameserver::handlers::AIFactory ArminosDrakyAI_aiFactory;
::aion::gameserver::handlers::AIFactory EmpyreanAdministratorArminosAI_aiFactory;
::aion::gameserver::handlers::AIFactory EmpyreanArbiterAI_aiFactory;
::aion::gameserver::handlers::AIFactory EmpyreanRecordKeeperAI_aiFactory;
::aion::gameserver::handlers::AIFactory KingConsierdAI_aiFactory;
::aion::gameserver::handlers::AIFactory MagePreceptorAI_aiFactory;
::aion::gameserver::handlers::AIFactory PriestPreceptorAI_aiFactory;
::aion::gameserver::handlers::AIFactory QueenAlukinaAI_aiFactory;
::aion::gameserver::handlers::AIFactory RM1337AI_aiFactory;
::aion::gameserver::handlers::AIFactory SparkOfDarknessAI_aiFactory;
::aion::gameserver::handlers::AIFactory SpectralWarriorAI_aiFactory;
::aion::gameserver::handlers::AIFactory StrangeCreatureAI_aiFactory;
::aion::gameserver::handlers::AIFactory TakunGojiraAI_aiFactory;
::aion::gameserver::handlers::AIFactory WarriorPreceptorAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::empyreanCrucible;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using model::instance::StageType;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t SETPRO1 = model::DialogAction::SETPRO1;
constexpr int32_t SETPRO2 = model::DialogAction::SETPRO2;

/** the skills an npc starts, in order */
using Casts = std::vector<std::pair<int32_t, int32_t>>;

/** A probe exposing the protected events of an AI and recording the skills its npc starts */
template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleSpawned;
	using AI::handleDied;
	using AI::handleDespawned;
	using AI::handleBackHome;
	using AI::handleAttack;
	Casts casts;
	/** the test world's attack sends the npc home at once (no fight here); a case lets it go home explicitly */
	bool homeAllowed = false;
	void handleBackHome() override {
		if (homeAllowed)
			AI::handleBackHome();
	}
	void goHome() {
		homeAllowed = true;
		handleBackHome();
		homeAllowed = false;
	}
	void onStartUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override {
		casts.emplace_back(skillTemplate->getSkillId(), skillLevel);
		AI::onStartUseSkill(skillTemplate, skillLevel);
	}
};
template <class AI>
struct DialogProbe : AI {
	using AI::AI;
	using AI::handleDialogStart;
	using AI::handleSpawned;
};

class EmpyreanCrucibleAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		publishCrucibleData(context);
		instance = &create(EMPYREAN_CRUCIBLE, EmpyreanCrucibleInstance_instanceFactory);
		enter(*instance, 345.0f, 349.0f, 96.1f);
		instance->getInstanceHandler()->onEnterInstance(player());
		(*client)->clearSent();
	}

	void TearDown() override {
		InstanceHandlerTest::TearDown();
		resetCrucibleData();
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId) {
		runtime::Ptr<Npc> npc = spawnNpc(*instance, npcId, 346.0f, 349.0f, 96.1f, int8_t{0});
		InstanceKnownListPairing::pair(*npc, player());
		return npc;
	}

	/** SKILL_DATA with one plain template per id (I5AiTest.cpp's; the base fixture's TearDown resets it) */
	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="crucible" nameId="1" stack="CRUCIBLE" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="ALL" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	/** the npc's hp to a percentage, then the player's attack */
	template <class AI>
	void attackedAt(Npc& npc, AI& ai, int32_t hpPercent) {
		npc.getLifeStats()->setCurrentHpPercent(hpPercent);
		ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	}

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	int64_t npcSays(Npc& npc, int32_t msgId) {
		return countSent(serialized(SM_SYSTEM_MESSAGE(model::ChatType::NPC, runtime::Ptr<model::gameobjects::VisibleObject>(npc), msgId,
			std::vector<std::string>{})));
	}

	int64_t dialogs(Npc& npc, int32_t page) { return countSent(serialized(SM_DIALOG_WINDOW(npc.getObjectId(), page))); }

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	static int32_t casts(const Casts& list, int32_t skillId) {
		int32_t n = 0;
		for (const auto& [id, level] : list)
			n += id == skillId;
		return n;
	}

	xml::LoadContext skillContext;
	xml::LoadContext npcContext;
	world::WorldMapInstance* instance = nullptr;
};

/** The fourteen AION_AI factories build their classes */
TEST_F(EmpyreanCrucibleAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(217744);
	EXPECT_NE(dynamic_cast<ais::ArminosDrakyAI*>(ais::ArminosDrakyAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::EmpyreanAdministratorArminosAI*>(ais::EmpyreanAdministratorArminosAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::EmpyreanArbiterAI*>(ais::EmpyreanArbiterAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::EmpyreanRecordKeeperAI*>(ais::EmpyreanRecordKeeperAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::KingConsierdAI*>(ais::KingConsierdAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::MagePreceptorAI*>(ais::MagePreceptorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::PriestPreceptorAI*>(ais::PriestPreceptorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::QueenAlukinaAI*>(ais::QueenAlukinaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::RM1337AI*>(ais::RM1337AI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::SparkOfDarknessAI*>(ais::SparkOfDarknessAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::SpectralWarriorAI*>(ais::SpectralWarriorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::StrangeCreatureAI*>(ais::StrangeCreatureAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TakunGojiraAI*>(ais::TakunGojiraAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::WarriorPreceptorAI*>(ais::WarriorPreceptorAI_aiFactory(*npc).get()), nullptr);
}

/** ArminosDrakyAI.java:29-63: its route and its state; immune, AP, no thinking */
TEST_F(EmpyreanCrucibleAiTest, TheDraky) {
	// the draky as a BEAST: NpcAI's own REWARD_AP answer is false for it (not an AP-rewarding race), so the true is the draky's
	std::string templates = handlerNpcTemplatesXml();
	const std::string row = R"(<npc_template npc_id="217744")";
	const size_t at = templates.find(R"(race="DRAKAN")", templates.find(row));
	templates.replace(at, std::string(R"(race="DRAKAN")").size(), R"(race="BEAST")");
	dataholders::DataManager::NPC_DATA.resetForTests();
	dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcContext, templates));
	runtime::Ptr<Npc> draky = place(217744);
	ASSERT_EQ(draky->getRace(), model::Race::BEAST);
	EXPECT_FALSE(install<Probe<::aion::gameserver::handlers::ai::GeneralNpcAI>>(*place(217744)).ask(AIQuestion::REWARD_AP)) << "NpcAI's answer for a BEAST here";
	Probe<ais::ArminosDrakyAI>& ai = install<Probe<ais::ArminosDrakyAI>>(*draky);
	EXPECT_TRUE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP));
	EXPECT_FALSE(ai.canThink());
	ai.handleSpawned();
	EXPECT_EQ(draky->getSpawn()->getWalkerId(), std::optional<std::string>("300300001"));
	EXPECT_TRUE(draky->isInState(model::gameobjects::state::CreatureState::ACTIVE));
	EXPECT_EQ(countSent(serialized(SM_EMOTION(*draky, model::EmotionType::CHANGE_SPEED, 0, draky->getObjectId()))), 1);
}

/** EmpyreanAdministratorArminosAI.java:19-49: the bonus stage 3 announcements */
TEST_F(EmpyreanCrucibleAiTest, TheAdministratorOfBonusStage3) {
	runtime::Ptr<Npc> arminos = place(217744);
	install<Probe<ais::EmpyreanAdministratorArminosAI>>(*arminos).handleSpawned();
	after(7999);
	EXPECT_EQ(npcSays(*arminos, 1500247), 0);
	after(1);
	EXPECT_EQ(npcSays(*arminos, 1500247), 1);
	after(12000);
	EXPECT_EQ(npcSays(*arminos, 1500250), 1);
	after(40000);
	EXPECT_EQ(npcSays(*arminos, 1500251), 1);
	EXPECT_EQ(npcSays(*arminos, 1500252), 0);
}

/** :35-46: the bonus stage 4 announcements */
TEST_F(EmpyreanCrucibleAiTest, TheAdministratorOfBonusStage4) {
	runtime::Ptr<Npc> arminos = place(217749);
	install<Probe<ais::EmpyreanAdministratorArminosAI>>(*arminos).handleSpawned();
	after(16000);
	EXPECT_EQ(npcSays(*arminos, 1500252), 1);
	EXPECT_EQ(npcSays(*arminos, 1500253), 1);
	after(102000);
	EXPECT_EQ(npcSays(*arminos, 1500255), 1);
	EXPECT_EQ(npcSays(*arminos, 1500247), 0);
}

/** EmpyreanArbiterAI.java:28-89: without a ticket, the empty dialog; another action keeps the ticket */
TEST_F(EmpyreanCrucibleAiTest, TheArbiter) {
	runtime::Ptr<Npc> arbiter = place(799573);
	DialogProbe<ais::EmpyreanArbiterAI>& ai = install<DialogProbe<ais::EmpyreanArbiterAI>>(*arbiter);
	ai.handleDialogStart(player());
	EXPECT_EQ(dialogs(*arbiter, 0), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1, 0, 0));
	EXPECT_EQ(dialogs(*arbiter, 0), 1) << "no ticket: nothing";
	services::item::ItemService::addItem(player(), 186000124, 1);
	(*client)->clearSent();
	ai.handleDialogStart(player());
	EXPECT_EQ(dialogs(*arbiter, 1011), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO2, 0, 0));
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000124), 1);
}

/** :41-89: with a ticket the arbiter of an unlisted npc takes it, lifts the defeat and tells everyone */
TEST_F(EmpyreanCrucibleAiTest, TheArbiterTakesTheTicket) {
	services::item::ItemService::addItem(player(), 186000124, 1);
	runtime::Ptr<model::instance::playerreward::CruciblePlayerReward> reward = runtime::cast<model::instance::playerreward::CruciblePlayerReward>(
		instance->getInstanceHandler()->getInstanceScore()->getPlayerReward(player().getObjectId()));
	reward->setPlayerDefeated(true);
	runtime::Ptr<Npc> arbiter = place(205344); // no teleport for this id
	DialogProbe<ais::EmpyreanArbiterAI>& ai = install<DialogProbe<ais::EmpyreanArbiterAI>>(*arbiter);
	(*client)->clearSent();
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1, 0, 0));
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000124), 0);
	EXPECT_FALSE(reward->isPlayerDefeated());
	EXPECT_EQ(dialogs(*arbiter, 0), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_FRIENDLY_MOVE_COMBATAREA_IDARENA(player().getName()))), 1);
}

/** EmpyreanRecordKeeperAI.java:26-64: the keepers' announcements 1 s after their spawn */
TEST_F(EmpyreanCrucibleAiTest, TheKeepersAnnounceThemselves) {
	const std::vector<std::pair<int32_t, int32_t>> keepers{{799568, 1111460}, {799569, 1111461}, {205331, 1111462}, {205338, 1111463},
		{205339, 1111464}, {205340, 1111465}, {205341, 1111466}, {205342, 1111467}, {205343, 1111468}, {205344, 1111469}};
	for (const auto& [npcId, msgId] : keepers) {
		runtime::Ptr<Npc> keeper = place(npcId);
		install<DialogProbe<ais::EmpyreanRecordKeeperAI>>(*keeper).handleSpawned();
		after(999);
		EXPECT_EQ(npcSays(*keeper, msgId), 0) << npcId;
		after(1);
		EXPECT_EQ(npcSays(*keeper, msgId), 1) << npcId;
	}
	runtime::Ptr<Npc> first = place(799567);
	install<DialogProbe<ais::EmpyreanRecordKeeperAI>>(*first).handleSpawned();
	(*client)->clearSent();
	after(1000);
	EXPECT_TRUE(sent().empty()) << "799567 says nothing";
}

/** :71-133: each starting keeper starts its stage and goes */
TEST_F(EmpyreanCrucibleAiTest, TheKeepersStartTheirStages) {
	const std::vector<std::pair<int32_t, StageType>> keepers{{799567, StageType::START_STAGE_1_ROUND_1},
		{799568, StageType::START_STAGE_2_ROUND_1}, {799569, StageType::START_STAGE_3_ROUND_1}, {205331, StageType::START_STAGE_4_ROUND_1},
		{205332, StageType::START_STAGE_5_ROUND_1}, {205333, StageType::START_STAGE_6_ROUND_1}, {205334, StageType::START_STAGE_7_ROUND_1},
		{205335, StageType::START_STAGE_8_ROUND_1}, {205336, StageType::START_STAGE_9_ROUND_1}};
	for (const auto& [npcId, stage] : keepers) {
		runtime::Ptr<Npc> keeper = place(npcId);
		DialogProbe<ais::EmpyreanRecordKeeperAI>& ai = install<DialogProbe<ais::EmpyreanRecordKeeperAI>>(*keeper);
		(*client)->clearSent();
		ai.handleDialogStart(player());
		EXPECT_EQ(dialogs(*keeper, 1011), 1) << npcId;
		EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1, 0, 0));
		EXPECT_EQ(dialogs(*keeper, 0), 1) << npcId;
		EXPECT_EQ(instance->getInstanceHandler()->getStage(), stage) << npcId;
		EXPECT_FALSE(keeper->isSpawned()) << npcId;
	}
}

/** :127-131, 71-80: another action, or SETPRO2 for another keeper, does nothing */
TEST_F(EmpyreanCrucibleAiTest, OtherActionsDoNothing) {
	runtime::Ptr<Npc> keeper = place(799568);
	DialogProbe<ais::EmpyreanRecordKeeperAI>& ai = install<DialogProbe<ais::EmpyreanRecordKeeperAI>>(*keeper);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO2, 0, 0));
	EXPECT_TRUE(keeper->isSpawned());
	EXPECT_EQ(instance->getInstanceHandler()->getStage(), StageType::DEFAULT);
	EXPECT_EQ(dialogs(*keeper, 0), 0);
}

/** :121-123: the last keeper gives the score and goes */
TEST_F(EmpyreanCrucibleAiTest, TheLastKeeperGivesTheScore) {
	runtime::Ptr<Npc> keeper = place(205344);
	install<DialogProbe<ais::EmpyreanRecordKeeperAI>>(*keeper).onDialogSelect(player(), SETPRO1, 0, 0);
	EXPECT_FALSE(keeper->isSpawned());
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000130), 350);
}

/** KingConsierdAI.java:53-114: the opening skills, the bloodthirst after 3 minutes, the bombs from 75 % with adds below 50 % */
TEST_F(EmpyreanCrucibleAiTest, KingConsierd) {
	publishSkills({19691, 17954, 19624, 17951, 17952, 19690});
	runtime::Ptr<Npc> king = place(217567);
	Probe<ais::KingConsierdAI>& ai = install<Probe<ais::KingConsierdAI>>(*king);
	attackedAt(*king, ai, 100);
	after(2000);
	EXPECT_EQ(ai.casts, (Casts{{19691, 1}}));
	after(4000);
	EXPECT_EQ(ai.casts, (Casts{{19691, 1}, {17954, 29}}));
	attackedAt(*king, ai, 100);
	after(1);
	EXPECT_EQ(ai.casts.size(), 2u) << "the opening once";
	attackedAt(*king, ai, 45);
	EXPECT_EQ(casts(ai.casts, 17951), 0) << "the round's first run is a task";
	after(1);
	EXPECT_EQ(casts(ai.casts, 17951), 1) << "75 %: the skill round at once";
	after(3499);
	EXPECT_EQ(count(282378), 2u) << "below 50 %: two adds";
	after(2000);
	EXPECT_EQ(casts(ai.casts, 17952), 1);
	after(180000 - 2000 - 4000 - 1 - 1 - 3499 - 2000 - 1);
	EXPECT_EQ(casts(ai.casts, 19624), 0);
	after(1);
	EXPECT_EQ(casts(ai.casts, 19624), 1) << "the bloodthirst at 3 minutes";
	attackedAt(*king, ai, 20);
	EXPECT_EQ(casts(ai.casts, 19690), 1) << "25 %";
	ai.handleDied();
	EXPECT_EQ(count(282378), 0u) << "the adds go with him";
	const size_t before = ai.casts.size();
	after(60000);
	EXPECT_EQ(ai.casts.size(), before) << "the skill round is cancelled";
}

/** :45-51, 98-114: the king's adds come only below 50 %; going home cancels the round and takes them */
TEST_F(EmpyreanCrucibleAiTest, KingConsierdGoesHome) {
	publishSkills({19691, 17954, 19624, 17951, 17952, 19690});
	runtime::Ptr<Npc> king = place(217567);
	Probe<ais::KingConsierdAI>& ai = install<Probe<ais::KingConsierdAI>>(*king);
	attackedAt(*king, ai, 70);
	after(3500);
	EXPECT_EQ(count(282378), 0u) << "70 %: no adds";
	place(282378);
	ai.goHome();
	EXPECT_EQ(count(282378), 0u);
	after(200000);
	EXPECT_EQ(casts(ai.casts, 19624), 0) << "the bloodthirst is cancelled";
	EXPECT_EQ(casts(ai.casts, 17951), 1) << "the round is cancelled";
}

/** MagePreceptorAI.java:46-103: the phases, the images at 50 % and their end with her */
TEST_F(EmpyreanCrucibleAiTest, TheMagePreceptor) {
	publishSkills({19605, 19606, 19609});
	runtime::Ptr<Npc> mage = place(217653);
	Probe<ais::MagePreceptorAI>& ai = install<Probe<ais::MagePreceptorAI>>(*mage);
	attackedAt(*mage, ai, 70);
	EXPECT_EQ(ai.casts, (Casts{{19605, 10}}));
	attackedAt(*mage, ai, 45);
	EXPECT_EQ(casts(ai.casts, 19606), 1);
	after(3000);
	EXPECT_EQ(casts(ai.casts, 19609), 1);
	after(4499);
	EXPECT_EQ(count(282364), 0u);
	after(1);
	EXPECT_EQ(count(282364) + count(282363), 2u);
	after(2000);
	EXPECT_EQ(casts(ai.casts, 19605), 2);
	attackedAt(*mage, ai, 20);
	EXPECT_EQ(casts(ai.casts, 19606), 2);
	after(15000);
	EXPECT_EQ(casts(ai.casts, 19605), 5) << "25 %: three more at 3, 9 and 15 s";
	ai.handleDied();
	EXPECT_EQ(count(282364) + count(282363), 0u);
}

/** :27-31, 39-43: the images go when she despawns or goes home; a dead mage casts no scheduled skill */
TEST_F(EmpyreanCrucibleAiTest, TheMagePreceptorsEnds) {
	publishSkills({19605, 19606, 19609});
	runtime::Ptr<Npc> mage = place(217653);
	Probe<ais::MagePreceptorAI>& ai = install<Probe<ais::MagePreceptorAI>>(*mage);
	place(282364);
	ai.goHome();
	EXPECT_EQ(count(282364), 0u);
	place(282363);
	ai.handleDespawned();
	EXPECT_EQ(count(282363), 0u);
}

/** PriestPreceptorAI.java:27-91: the buff after the spawn, the phases and the three soul-sick adds */
TEST_F(EmpyreanCrucibleAiTest, ThePriestPreceptor) {
	publishSkills({19612, 19611, 19610, 19614, 19594});
	runtime::Ptr<Npc> priest = place(217652);
	Probe<ais::PriestPreceptorAI>& ai = install<Probe<ais::PriestPreceptorAI>>(*priest);
	ai.handleSpawned();
	after(999);
	EXPECT_TRUE(ai.casts.empty());
	after(1);
	EXPECT_EQ(ai.casts, (Casts{{19612, 15}}));
	attackedAt(*priest, ai, 70);
	EXPECT_EQ(casts(ai.casts, 19611), 1);
	attackedAt(*priest, ai, 20);
	EXPECT_EQ(casts(ai.casts, 19610), 1);
	after(2000);
	EXPECT_EQ(casts(ai.casts, 19614), 1);
	after(5000);
	EXPECT_EQ(count(282366) + count(282367) + count(282368), 3u);
	after(1000);
	EXPECT_EQ(npcs(*instance, 282366)[0]->getLifeStats()->getHpPercentage(), 50) << "the adds at half hp";
}

/** QueenAlukinaAI.java:48-94: a skill at each phase, the follow-ups, the round from 25 % */
TEST_F(EmpyreanCrucibleAiTest, QueenAlukina) {
	publishSkills({17899, 17900, 17280, 17902, 17901});
	runtime::Ptr<Npc> queen = place(217651);
	Probe<ais::QueenAlukinaAI>& ai = install<Probe<ais::QueenAlukinaAI>>(*queen);
	attackedAt(*queen, ai, 70);
	EXPECT_EQ(ai.casts, (Casts{{17899, 41}}));
	after(4500);
	EXPECT_EQ(casts(ai.casts, 17900), 1);
	after(5500);
	EXPECT_EQ(npcSays(*queen, 340487), 1);
	after(8000);
	EXPECT_EQ(casts(ai.casts, 17899), 2);
	EXPECT_EQ(casts(ai.casts, 17900), 2);
	attackedAt(*queen, ai, 45);
	after(8000);
	EXPECT_EQ(casts(ai.casts, 17280), 1);
	EXPECT_EQ(casts(ai.casts, 17902), 1);
	attackedAt(*queen, ai, 20);
	after(4500);
	EXPECT_EQ(casts(ai.casts, 17901), 1);
	after(7500);
	EXPECT_EQ(casts(ai.casts, 17902), 3);
	after(20000);
	EXPECT_EQ(casts(ai.casts, 17901), 2) << "every 20 s";
	ai.handleDied();
	after(40000);
	EXPECT_EQ(casts(ai.casts, 17901), 2) << "cancelled";
}

/** RM1337AI.java:32-143: the greeting, the skill every 23 s, the sparks' minute from 75 %, the farewell */
TEST_F(EmpyreanCrucibleAiTest, RM1337) {
	publishSkills({19550, 19551, 19552, 19553});
	runtime::Ptr<Npc> rm = place(217590);
	Probe<ais::RM1337AI>& ai = install<Probe<ais::RM1337AI>>(*rm);
	ai.handleSpawned();
	after(2000);
	EXPECT_EQ(npcSays(*rm, 1500229), 1);
	attackedAt(*rm, ai, 80);
	EXPECT_TRUE(ai.casts.empty()) << "above 75 %: no sparks";
	after(9999);
	EXPECT_TRUE(ai.casts.empty());
	after(1);
	EXPECT_EQ(ai.casts, (Casts{{19550, 10}})) << "10 s after the first attack";
	attackedAt(*rm, ai, 70);
	after(1);
	EXPECT_EQ(casts(ai.casts, 19551), 1);
	EXPECT_EQ(npcSays(*rm, 1500230), 1);
	after(3999);
	const size_t sparks = count(282373);
	EXPECT_GE(sparks, 8u);
	EXPECT_LE(sparks, 12u);
	attackedAt(*rm, ai, 70);
	EXPECT_EQ(casts(ai.casts, 19551), 1) << "the event once";
	ai.handleDied();
	EXPECT_EQ(npcSays(*rm, 1500231), 1);
	const size_t before = ai.casts.size();
	after(120000);
	EXPECT_EQ(ai.casts.size(), before) << "both tasks cancelled";
}

/** SparkOfDarknessAI.java and StrangeCreatureAI.java: a skill at 0.5 s, gone at 6.5 s */
TEST_F(EmpyreanCrucibleAiTest, SparksAndStrangeCreatures) {
	publishSkills({19554, 17914});
	runtime::Ptr<Npc> spark = place(282373);
	Probe<ais::SparkOfDarknessAI>& sparkAi = install<Probe<ais::SparkOfDarknessAI>>(*spark);
	runtime::Ptr<Npc> creature = place(282378);
	Probe<ais::StrangeCreatureAI>& creatureAi = install<Probe<ais::StrangeCreatureAI>>(*creature);
	sparkAi.handleSpawned();
	creatureAi.handleSpawned();
	after(499);
	EXPECT_TRUE(sparkAi.casts.empty());
	after(1);
	EXPECT_EQ(sparkAi.casts, (Casts{{19554, 1}}));
	EXPECT_EQ(creatureAi.casts, (Casts{{17914, 34}}));
	EXPECT_EQ(npcSays(*creature, 341444), 1);
	after(5999);
	EXPECT_TRUE(spark->isSpawned());
	EXPECT_TRUE(creature->isSpawned());
	after(1);
	EXPECT_FALSE(spark->isSpawned());
	EXPECT_FALSE(creature->isSpawned());
}

/** SpectralWarriorAI.java:25-53: at 50 % round 5 starts and the fallen it knows rise */
TEST_F(EmpyreanCrucibleAiTest, TheSpectralWarrior) {
	runtime::Ptr<Npc> warrior = place(217570);
	runtime::Ptr<Npc> fallen = place(205413);
	runtime::Ptr<Npc> fallen2 = place(205414);
	InstanceKnownListPairing::pair(*warrior, *fallen);
	InstanceKnownListPairing::pair(*warrior, *fallen2);
	Probe<ais::SpectralWarriorAI>& ai = install<Probe<ais::SpectralWarriorAI>>(*warrior);
	attackedAt(*warrior, ai, 60);
	EXPECT_EQ(instance->getInstanceHandler()->getStage(), StageType::DEFAULT);
	attackedAt(*warrior, ai, 50);
	EXPECT_EQ(instance->getInstanceHandler()->getStage(), StageType::START_STAGE_6_ROUND_5);
	after(1999);
	EXPECT_TRUE(fallen->isSpawned());
	after(1);
	EXPECT_FALSE(fallen->isSpawned());
	EXPECT_FALSE(fallen2->isSpawned());
	EXPECT_EQ(count(217576), 1u);
	EXPECT_EQ(count(217577), 1u);
}

/** TakunGojiraAI.java:22-35: half a second after its spawn a twin hates the other */
TEST_F(EmpyreanCrucibleAiTest, TheTwins) {
	runtime::Ptr<Npc> takun = place(217596);
	runtime::Ptr<Npc> gojira = place(217597);
	InstanceKnownListPairing::pair(*takun, *gojira);
	// the twins' GENERAL tribe hostile to itself (the aggro list's awareness; the real tribes are)
	dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
	dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(context,
		R"(<tribe_relations><tribe name="GENERAL"><hostile>PC PC_DARK GENERAL</hostile></tribe><tribe name="PC"><none>GENERAL</none></tribe>)"
		R"(<tribe name="PC_DARK"><none>GENERAL</none></tribe></tribe_relations>)"));
	Probe<ais::TakunGojiraAI>& ai = install<Probe<ais::TakunGojiraAI>>(*takun);
	ai.handleSpawned();
	after(499);
	EXPECT_FALSE(takun->getAggroList().isHating(*gojira));
	after(1);
	EXPECT_EQ(takun->getAggroList().getHate(*gojira), 1000000);
}

/** WarriorPreceptorAI.java:30-92: every 30 s once attacked, two skills; its death says its line and cancels */
TEST_F(EmpyreanCrucibleAiTest, TheWarriorPreceptor) {
	publishSkills({19595, 19596});
	runtime::Ptr<Npc> warrior = place(217651);
	Probe<ais::WarriorPreceptorAI>& ai = install<Probe<ais::WarriorPreceptorAI>>(*warrior);
	attackedAt(*warrior, ai, 100);
	after(29999);
	EXPECT_TRUE(ai.casts.empty());
	after(1);
	EXPECT_EQ(npcSays(*warrior, 1500207), 1);
	EXPECT_EQ(ai.casts, (Casts{{19595, 10}}));
	after(6000);
	EXPECT_EQ(ai.casts, (Casts{{19595, 10}, {19596, 15}}));
	attackedAt(*warrior, ai, 100);
	after(24000);
	EXPECT_EQ(casts(ai.casts, 19595), 2) << "one task, every 30 s";
	ai.goHome();
	after(60000);
	EXPECT_EQ(casts(ai.casts, 19595), 2) << "home: cancelled";
	attackedAt(*warrior, ai, 100);
	after(30000);
	EXPECT_EQ(casts(ai.casts, 19595), 3) << "home resets: the next attack starts again";
	ai.handleDied();
	EXPECT_EQ(npcSays(*warrior, 1500208), 1);
	after(60000);
	EXPECT_EQ(casts(ai.casts, 19595), 3);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
