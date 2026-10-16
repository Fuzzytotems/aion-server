// P6-I2 Dark Poeta's AIs (game-server/data/handlers/ai/instance/darkPoeta) on npcs of a real Dark Poeta instance, each AI installed on its
// npc and driven through its protected events. Skills are plain templates (I5AiTest.cpp's); a probe records the skills its npc starts. The
// test world sends an attacked npc home at once (no fight here): the probes go home only when a case asks. Left out: the wounded Scar's
// walk (no walker route), the drana lump's own skill (its effect), the wipe skill's queue (the npc skill queue runs in a fight).

#include "I2TestSupport.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/BalaurBarricadeAI.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/CalindiFlamelordAI.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/DranaLumpAI.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/MarabataAI.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/MarabataControllerAI.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/TahabataGargoyleAI.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/TahabataPyrelordAI.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/TelepathyControllerAI.h"
#include "aion/gameserver/handlers/ai/instance/darkPoeta/WoundedScarAI.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {
::aion::gameserver::handlers::AIFactory BalaurBarricadeAI_aiFactory;
::aion::gameserver::handlers::AIFactory CalindiFlamelordAI_aiFactory;
::aion::gameserver::handlers::AIFactory DranaLumpAI_aiFactory;
::aion::gameserver::handlers::AIFactory MarabataAI_aiFactory;
::aion::gameserver::handlers::AIFactory MarabataControllerAI_aiFactory;
::aion::gameserver::handlers::AIFactory TahabataGargoyleAI_aiFactory;
::aion::gameserver::handlers::AIFactory TahabataPyrelordAI_aiFactory;
::aion::gameserver::handlers::AIFactory TelepathyControllerAI_aiFactory;
::aion::gameserver::handlers::AIFactory WoundedScarAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::darkPoeta

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::darkPoeta;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

using PoetaCasts = std::vector<std::pair<int32_t, int32_t>>;

template <class AI>
struct PoetaProbe : AI {
	using AI::AI;
	using AI::handleSpawned;
	using AI::handleDied;
	using AI::handleDespawned;
	using AI::handleAttack;
	PoetaCasts casts;
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
struct PoetaDialogProbe : AI {
	using AI::AI;
	using AI::handleDialogStart;
};

class DarkPoetaAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(DARK_POETA, DarkPoetaInstance_instanceFactory);
		enter(*instance, 1180.0f, 1235.0f, 143.0f);
		(*client)->clearSent();
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float dx = 1.0f) {
		runtime::Ptr<Npc> npc = spawnNpc(*instance, npcId, 1180.0f + dx, 1235.0f, 143.0f, int8_t{0});
		InstanceKnownListPairing::pair(*npc, player());
		return npc;
	}

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="poeta" nameId="1" stack="POETA" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="ALL" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	static const skillengine::model::SkillTemplate* skill(int32_t id) { return dataholders::DataManager::SKILL_DATA->getSkillTemplate(id); }

	template <class AI>
	void attackedAt(Npc& npc, AI& ai, int32_t hpPercent) {
		npc.getLifeStats()->setCurrentHpPercent(hpPercent);
		ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	}

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

/** The nine AION_AI factories build their classes */
TEST_F(DarkPoetaAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(215262);
	EXPECT_NE(dynamic_cast<ais::BalaurBarricadeAI*>(ais::BalaurBarricadeAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::CalindiFlamelordAI*>(ais::CalindiFlamelordAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DranaLumpAI*>(ais::DranaLumpAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::MarabataAI*>(ais::MarabataAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::MarabataControllerAI*>(ais::MarabataControllerAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TahabataGargoyleAI*>(ais::TahabataGargoyleAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TahabataPyrelordAI*>(ais::TahabataPyrelordAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TelepathyControllerAI*>(ais::TelepathyControllerAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::WoundedScarAI*>(ais::WoundedScarAI_aiFactory(*npc).get()), nullptr);
}

/** BalaurBarricadeAI.java: two protectors at 50 %, the second pair at 10 %, again after going home */
TEST_F(DarkPoetaAiTest, TheBarricade) {
	runtime::Ptr<Npc> barricade = place(700517);
	PoetaProbe<ais::BalaurBarricadeAI>& ai = install<PoetaProbe<ais::BalaurBarricadeAI>>(*barricade);
	attackedAt(*barricade, ai, 60);
	EXPECT_EQ(count(215262), 0u);
	attackedAt(*barricade, ai, 50);
	EXPECT_EQ(count(215262), 2u);
	attackedAt(*barricade, ai, 10);
	EXPECT_EQ(count(214883), 1u);
	EXPECT_EQ(count(215263), 1u);
	ai.goHome();
	attackedAt(*barricade, ai, 40);
	EXPECT_EQ(count(215262), 4u) << "home resets the phases";
	runtime::Ptr<Npc> other = place(700518, 2.0f);
	PoetaProbe<ais::BalaurBarricadeAI>& otherAi = install<PoetaProbe<ais::BalaurBarricadeAI>>(*other);
	attackedAt(*other, otherAi, 40);
	EXPECT_EQ(count(215262), 4u) << "700518 has no protectors";
}

/** CalindiFlamelordAI.java: the announcement, the flames by her hp, the end of the time */
TEST_F(DarkPoetaAiTest, Calindi) {
	publishSkills({18233, 19679, 1});
	runtime::Ptr<Npc> calindi = place(215281);
	PoetaProbe<ais::CalindiFlamelordAI>& ai = install<PoetaProbe<ais::CalindiFlamelordAI>>(*calindi);
	ai.handleSpawned();
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_A_RANK_BATTLE_TIME())), 1);
	calindi->getLifeStats()->setCurrentHpPercent(50);
	ai.onEndUseSkill(skill(18233), 1);
	EXPECT_EQ(count(281267), 4u);
	calindi->getLifeStats()->setCurrentHpPercent(70);
	ai.onEndUseSkill(skill(18233), 1);
	EXPECT_EQ(count(281268), 2u) << "outside 30-60 %: two near her";
	calindi->getLifeStats()->setCurrentHpPercent(29);
	ai.onEndUseSkill(skill(18233), 1);
	EXPECT_EQ(count(281268), 4u);
	ai.onEndUseSkill(skill(1), 1);
	EXPECT_TRUE(calindi->isSpawned());
	ai.onEndUseSkill(skill(19679), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_A_RANK_BATTLE_END())), 1);
	EXPECT_FALSE(calindi->isSpawned());
}

/** TahabataPyrelordAI.java: the announcement, his adds by skill, the end of the time */
TEST_F(DarkPoetaAiTest, Tahabata) {
	publishSkills({18236, 18241, 19679});
	runtime::Ptr<Npc> tahabata = place(215280);
	PoetaProbe<ais::TahabataPyrelordAI>& ai = install<PoetaProbe<ais::TahabataPyrelordAI>>(*tahabata);
	ai.handleSpawned();
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_S_RANK_BATTLE_TIME())), 1);
	ai.onEndUseSkill(skill(18236), 1);
	EXPECT_EQ(count(281258), 4u);
	ai.onEndUseSkill(skill(18241), 1);
	EXPECT_EQ(count(281259), 4u);
	ai.onEndUseSkill(skill(19679), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_S_RANK_BATTLE_END())), 1);
	EXPECT_FALSE(tahabata->isSpawned());
}

/** TahabataGargoyleAI.java: gone after 18219 */
TEST_F(DarkPoetaAiTest, TheGargoyle) {
	publishSkills({18219, 18220});
	runtime::Ptr<Npc> gargoyle = place(281258);
	PoetaProbe<ais::TahabataGargoyleAI>& ai = install<PoetaProbe<ais::TahabataGargoyleAI>>(*gargoyle);
	ai.onEndUseSkill(skill(18220), 1);
	EXPECT_TRUE(gargoyle->isSpawned());
	ai.onEndUseSkill(skill(18219), 1);
	EXPECT_FALSE(gargoyle->isSpawned());
}

/** DranaLumpAI.java: a boss detected; its skill's end removes the lump */
TEST_F(DarkPoetaAiTest, TheDranaLump) {
	publishSkills({18536, 18537});
	runtime::Ptr<Npc> lump = place(700520);
	PoetaProbe<ais::DranaLumpAI>& ai = install<PoetaProbe<ais::DranaLumpAI>>(*lump);
	runtime::Ptr<Npc> boss = place(214880, 1.5f);
	ai.handleCreatureDetected(*boss);
	after(4000);
	EXPECT_EQ(ai.casts, (PoetaCasts{{18536, 46}})) << "the boss within 2 m: the lump's skill";
	EXPECT_FALSE(lump->isSpawned()) << "the skill's end (no cast time here) removes the lump";
}

/** :21-31, 41-45: another npc is not watched; only 18536's end removes a lump */
TEST_F(DarkPoetaAiTest, TheDranaLumpIgnoresOthers) {
	publishSkills({18536, 18537});
	runtime::Ptr<Npc> lump = place(700520);
	PoetaProbe<ais::DranaLumpAI>& ai = install<PoetaProbe<ais::DranaLumpAI>>(*lump);
	runtime::Ptr<Npc> other = place(214843, 1.5f);
	ai.handleCreatureDetected(*other);
	ai.handleCreatureDetected(player());
	after(8000);
	EXPECT_TRUE(ai.casts.empty()) << "another npc or a player: nothing";
	ai.onEndUseSkill(skill(18537), 1);
	EXPECT_TRUE(lump->isSpawned());
	ai.onEndUseSkill(skill(18536), 1);
	EXPECT_FALSE(lump->isSpawned());
}

/** :32-38: a boss further away is checked again every 4 s */
TEST_F(DarkPoetaAiTest, TheDranaLumpWaits) {
	publishSkills({18536});
	runtime::Ptr<Npc> lump = place(700520);
	PoetaProbe<ais::DranaLumpAI>& ai = install<PoetaProbe<ais::DranaLumpAI>>(*lump);
	runtime::Ptr<Npc> boss = place(215388, 6.0f);
	ai.handleCreatureDetected(*boss);
	after(8000);
	EXPECT_TRUE(ai.casts.empty());
	boss->getPosition()->setXYZH(1181.5f, 1235.0f, 143.0f, int8_t{0});
	after(4000);
	EXPECT_EQ(ai.casts.size(), 1u);
}

/** MarabataAI.java: her three boosters, brought back every 30 s once attacked; gone with her */
TEST_F(DarkPoetaAiTest, Marabata) {
	runtime::Ptr<Npc> marabata = place(214849);
	PoetaProbe<ais::MarabataAI>& ai = install<PoetaProbe<ais::MarabataAI>>(*marabata);
	ai.handleSpawned();
	EXPECT_EQ(count(700439) + count(700440) + count(700441), 3u);
	npcs(*instance, 700440)[0]->getController().delete_();
	attackedAt(*marabata, ai, 90);
	after(29999);
	EXPECT_EQ(count(700440), 0u);
	after(1);
	EXPECT_EQ(count(700440), 1u) << "the dead booster back after 30 s";
	EXPECT_EQ(count(700439), 1u) << "a living one is not doubled";
	ai.handleDied();
	npcs(*instance, 700441)[0]->getController().delete_();
	after(30000);
	EXPECT_EQ(count(700441), 0u) << "her death cancels the check";
}

/** :63-82: the other two Marabatas' boosters */
TEST_F(DarkPoetaAiTest, TheOtherMarabatas) {
	install<PoetaProbe<ais::MarabataAI>>(*place(214850)).handleSpawned();
	EXPECT_EQ(count(700442) + count(700443) + count(700444), 3u);
	install<PoetaProbe<ais::MarabataAI>>(*place(214851, 2.0f)).handleSpawned();
	EXPECT_EQ(count(700445) + count(700446) + count(700447), 3u);
}

/** MarabataControllerAI.java: the booster's skill after 3.5 s, its boss buffed after 10 s; its death removes it */
TEST_F(DarkPoetaAiTest, TheBooster) {
	publishSkills({18554, 18555, 18553, 18556, 18110});
	runtime::Ptr<Npc> marabata = place(214849);
	PoetaProbe<ais::MarabataAI>& boss = install<PoetaProbe<ais::MarabataAI>>(*marabata);
	runtime::Ptr<Npc> booster = place(700440, 2.0f);
	PoetaProbe<ais::MarabataControllerAI>& ai = install<PoetaProbe<ais::MarabataControllerAI>>(*booster);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	ai.handleSpawned();
	after(3499);
	EXPECT_TRUE(ai.casts.empty());
	after(1);
	EXPECT_EQ(ai.casts, (PoetaCasts{{18554, 1}}));
	after(6500);
	EXPECT_EQ(boss.casts, (PoetaCasts{{18556, 1}})) << "the boss buffed";
	ai.handleDied();
	EXPECT_FALSE(booster->isSpawned());
}

/** :65-73: the third booster kind casts 18553 and buffs with 18110; the second kind buffs nothing */
TEST_F(DarkPoetaAiTest, TheOtherBoosters) {
	publishSkills({18554, 18555, 18553, 18556, 18110});
	runtime::Ptr<Npc> marabata = place(214850);
	PoetaProbe<ais::MarabataAI>& boss = install<PoetaProbe<ais::MarabataAI>>(*marabata);
	PoetaProbe<ais::MarabataControllerAI>& third = install<PoetaProbe<ais::MarabataControllerAI>>(*place(700442, 2.0f));
	PoetaProbe<ais::MarabataControllerAI>& second = install<PoetaProbe<ais::MarabataControllerAI>>(*place(700444, 3.0f));
	third.handleSpawned();
	second.handleSpawned();
	after(10000);
	EXPECT_EQ(third.casts, (PoetaCasts{{18553, 1}}));
	EXPECT_EQ(second.casts, (PoetaCasts{{18555, 1}}));
	EXPECT_EQ(boss.casts, (PoetaCasts{{18110, 1}})) << "only the third kind buffs";
}

/** TelepathyControllerAI.java: once attacked, an add every minute; home cancels */
TEST_F(DarkPoetaAiTest, TheTelepathyController) {
	runtime::Ptr<Npc> controller = place(214864);
	PoetaProbe<ais::TelepathyControllerAI>& ai = install<PoetaProbe<ais::TelepathyControllerAI>>(*controller);
	attackedAt(*controller, ai, 90);
	after(59999);
	EXPECT_EQ(count(281150) + count(281334), 0u);
	after(1);
	EXPECT_EQ(count(281150) + count(281334), 1u);
	attackedAt(*controller, ai, 90);
	after(60000);
	EXPECT_EQ(count(281150) + count(281334), 2u) << "one task";
	ai.goHome();
	after(120000);
	EXPECT_EQ(count(281150) + count(281334), 2u) << "home cancels";
	attackedAt(*controller, ai, 90);
	after(60000);
	EXPECT_EQ(count(281150) + count(281334), 3u) << "after home the next attack starts again";
	ai.handleDied();
	after(120000);
	EXPECT_EQ(count(281150) + count(281334), 3u);
}

/** WoundedScarAI.java: no thinking; talked to, it takes its route invulnerable */
TEST_F(DarkPoetaAiTest, TheWoundedScar) {
	runtime::Ptr<Npc> scar = place(215386);
	PoetaDialogProbe<ais::WoundedScarAI>& ai = install<PoetaDialogProbe<ais::WoundedScarAI>>(*scar);
	EXPECT_FALSE(ai.canThink());
	ai.handleDialogStart(player());
	EXPECT_EQ(scar->getSpawn()->getWalkerId(), std::optional<std::string>("9C671D72B623B88FDFAFF9E2AF491976B84AE720"));
	EXPECT_EQ(scar->getType(player()), model::CreatureType::INVULNERABLE);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
