// P6I-13 cp2 (docs/design/p6-instances-plan.md, I4 split): the AIs of dragonLordsRefuge (game-server/data/handlers/ai/instance) on npcs of a
// real Dragon Lord's Refuge instance, each AI installed with replaceAi. Left to a gate: the casts and their effects (the buffs, the
// atrocities' and the summons' skills, the surkanas' reflect), the walks and the queued skills' use.

#include "I4TestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalculatedAtrocityAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalindiFlamelordAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalindiSummonsAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalindiSurkanaAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/DivisiveCreationAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/EmpyreanLordAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/GravityCrusherAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/GravityTornadoAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/HM_CalindiFlamelordAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/HM_TiamatWeakenedDragonAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_DrakanNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_KahrunTalkAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_NPC_TiamatAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/ThickDustAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatDragonAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatSkillHelperAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatWeakenedDragonAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatsIncarnationAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatsIncarnationSpawnsAI.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/UltimateAtrocityAI.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {
::aion::gameserver::handlers::AIFactory CalculatedAtrocityAI_aiFactory;
::aion::gameserver::handlers::AIFactory CalindiFlamelordAI_aiFactory;
::aion::gameserver::handlers::AIFactory CalindiSummonsAI_aiFactory;
::aion::gameserver::handlers::AIFactory CalindiSurkanaAI_aiFactory;
::aion::gameserver::handlers::AIFactory DivisiveCreationAI_aiFactory;
::aion::gameserver::handlers::AIFactory EmpyreanLordAI_aiFactory;
::aion::gameserver::handlers::AIFactory GravityCrusherAI_aiFactory;
::aion::gameserver::handlers::AIFactory GravityTornadoAI_aiFactory;
::aion::gameserver::handlers::AIFactory HM_CalindiFlamelordAI_aiFactory;
::aion::gameserver::handlers::AIFactory HM_TiamatWeakenedDragonAI_aiFactory;
::aion::gameserver::handlers::AIFactory IDTiamat_2_DrakanNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory IDTiamat_2_KahrunTalkAI_aiFactory;
::aion::gameserver::handlers::AIFactory IDTiamat_2_NPC_TiamatAI_aiFactory;
::aion::gameserver::handlers::AIFactory ThickDustAI_aiFactory;
::aion::gameserver::handlers::AIFactory TiamatDragonAI_aiFactory;
::aion::gameserver::handlers::AIFactory TiamatSkillHelperAI_aiFactory;
::aion::gameserver::handlers::AIFactory TiamatWeakenedDragonAI_aiFactory;
::aion::gameserver::handlers::AIFactory TiamatsIncarnationAI_aiFactory;
::aion::gameserver::handlers::AIFactory TiamatsIncarnationSpawnsAI_aiFactory;
::aion::gameserver::handlers::AIFactory UltimateAtrocityAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::dragonLordsRefuge;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using model::templates::item::ItemAttackType;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// the protected handlers the cases drive directly (Java: the AI's events)
template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleSpawned;
};
struct NpcTiamatProbe : ais::IDTiamat_2_NPC_TiamatAI {
	using IDTiamat_2_NPC_TiamatAI::IDTiamat_2_NPC_TiamatAI;
	using IDTiamat_2_NPC_TiamatAI::handleBeforeSpawned;
};
struct KahrunProbe : ais::IDTiamat_2_KahrunTalkAI {
	using IDTiamat_2_KahrunTalkAI::IDTiamat_2_KahrunTalkAI;
	using IDTiamat_2_KahrunTalkAI::handleDialogStart;
};
struct IncarnationProbe : ais::TiamatsIncarnationAI {
	using TiamatsIncarnationAI::TiamatsIncarnationAI;
	using TiamatsIncarnationAI::handleDespawned;
};
struct WeakenedProbe : ais::TiamatWeakenedDragonAI {
	using TiamatWeakenedDragonAI::TiamatWeakenedDragonAI;
	using TiamatWeakenedDragonAI::handleAttack;
	using TiamatWeakenedDragonAI::handleBackHome;
};
struct HmWeakenedProbe : ais::HM_TiamatWeakenedDragonAI {
	using HM_TiamatWeakenedDragonAI::HM_TiamatWeakenedDragonAI;
	using HM_TiamatWeakenedDragonAI::handleBackHome;
	using HM_TiamatWeakenedDragonAI::handleSpawned;
};

class DragonLordsRefugeAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(DRAGON_LORDS_REFUGE, DragonLordsRefugeInstance_instanceFactory);
		enter(*instance, 300.0f, 300.0f, 300.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 302.0f, float y = 300.0f) { return spawnNpc(*instance, npcId, x, y, 300.0f, int8_t{0}); }

	size_t count(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(*instance, id).size();
		return n;
	}

	static bool queued(Npc& npc, int32_t skillId) {
		return npc.hasQueuedSkill([skillId](model::skill::NpcSkillEntry& entry) { return entry.getSkillId() == skillId; });
	}

	const skillengine::model::SkillTemplate* skill(int32_t id) { return dataholders::DataManager::SKILL_DATA->getSkillTemplate(id); }

	int64_t sent(SM_SYSTEM_MESSAGE&& msg) { return countSent(serialized(std::move(msg))); }

	world::WorldMapInstance* instance = nullptr;
};

/** The twenty AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(DragonLordsRefugeAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::CalculatedAtrocityAI*>(ais::CalculatedAtrocityAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::CalindiFlamelordAI*>(ais::CalindiFlamelordAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::CalindiSummonsAI*>(ais::CalindiSummonsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::CalindiSurkanaAI*>(ais::CalindiSurkanaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DivisiveCreationAI*>(ais::DivisiveCreationAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::EmpyreanLordAI*>(ais::EmpyreanLordAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::GravityCrusherAI*>(ais::GravityCrusherAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::GravityTornadoAI*>(ais::GravityTornadoAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::HM_CalindiFlamelordAI*>(ais::HM_CalindiFlamelordAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::HM_TiamatWeakenedDragonAI*>(ais::HM_TiamatWeakenedDragonAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::IDTiamat_2_DrakanNpcAI*>(ais::IDTiamat_2_DrakanNpcAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::IDTiamat_2_KahrunTalkAI*>(ais::IDTiamat_2_KahrunTalkAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::IDTiamat_2_NPC_TiamatAI*>(ais::IDTiamat_2_NPC_TiamatAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ThickDustAI*>(ais::ThickDustAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TiamatDragonAI*>(ais::TiamatDragonAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TiamatSkillHelperAI*>(ais::TiamatSkillHelperAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TiamatWeakenedDragonAI*>(ais::TiamatWeakenedDragonAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TiamatsIncarnationAI*>(ais::TiamatsIncarnationAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TiamatsIncarnationSpawnsAI*>(ais::TiamatsIncarnationSpawnsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::UltimateAtrocityAI*>(ais::UltimateAtrocityAI_aiFactory(*npc).get()), nullptr);
}

/** The answers: no decay, respawn or rewards for the summons; no loot and bound checks for the weakened Tiamat */
TEST_F(DragonLordsRefugeAiTest, TheAnswers) {
	auto& surkana = install<ais::CalindiSurkanaAI>(*place(900200));
	EXPECT_FALSE(surkana.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(surkana.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(surkana.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	auto& crusher = install<ais::GravityCrusherAI>(*place(900200));
	EXPECT_FALSE(crusher.ask(AIQuestion::ALLOW_RESPAWN));
	auto& creation = install<ais::DivisiveCreationAI>(*place(900200));
	EXPECT_FALSE(creation.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_EQ(creation.modifyAttackType(ItemAttackType::PHYSICAL), ItemAttackType::MAGICAL_EARTH);
	EXPECT_FLOAT_EQ(creation.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f);
	auto& incarnation = install<ais::TiamatsIncarnationAI>(*place(900200));
	EXPECT_FALSE(incarnation.ask(AIQuestion::ALLOW_DECAY));
	auto& dragon = install<ais::TiamatDragonAI>(*place(900200));
	EXPECT_FALSE(dragon.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	auto& helper = install<ais::TiamatSkillHelperAI>(*place(900200));
	EXPECT_FALSE(helper.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_FLOAT_EQ(helper.modifyDamage(player(), 42.0f, nullptr), 0.0f);
	auto& weakened = install<ais::TiamatWeakenedDragonAI>(*place(900200));
	EXPECT_FALSE(weakened.ask(AIQuestion::REWARD_LOOT));
	EXPECT_TRUE(weakened.ask(AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKED));
	EXPECT_TRUE(weakened.ask(AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKING));
	EXPECT_EQ(weakened.modifyAttackType(ItemAttackType::PHYSICAL), ItemAttackType::MAGICAL_FIRE);
	auto& drakan = install<ais::IDTiamat_2_DrakanNpcAI>(*place(900200));
	EXPECT_FLOAT_EQ(drakan.modifyDamage(player(), 42.0f, nullptr), 42.0f);
}

/** GravityTornadoAI.java, TiamatsIncarnationSpawnsAI.java, CalindiSummonsAI.java: the spawned summons' answers (spawned: their despawn
 * cancels their task, which Java's null task would not survive) */
TEST_F(DragonLordsRefugeAiTest, TheSummonsAnswers) {
	publishSkills({20966, 20155, 20914});
	auto& tornado = install<Probe<ais::GravityTornadoAI>>(*place(283142));
	tornado.handleSpawned();
	EXPECT_FALSE(tornado.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(tornado.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(tornado.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	auto& spawns = install<Probe<ais::TiamatsIncarnationSpawnsAI>>(*place(282727));
	spawns.handleSpawned();
	EXPECT_FALSE(spawns.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(spawns.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(spawns.ask(AIQuestion::ALLOW_RESPAWN)) << "NpcAI's";
	auto& summon = install<Probe<ais::CalindiSummonsAI>>(*place(283133));
	summon.handleSpawned();
	EXPECT_FALSE(summon.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(summon.ask(AIQuestion::ALLOW_RESPAWN));
	executor().advance(std::chrono::milliseconds(7000));
}

/** ThickDustAI.java:20-23: it goes 10 s after its spawn */
TEST_F(DragonLordsRefugeAiTest, TheDustSettles) {
	runtime::Ptr<Npc> dust = place(900200);
	install<Probe<ais::ThickDustAI>>(*dust).handleSpawned();
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_TRUE(dust->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(dust->isSpawned());
}

/** IDTiamat_2_NPC_TiamatAI.java:21-33: peaceful; 20919 brings her dragon form (the next id) and she goes */
TEST_F(DragonLordsRefugeAiTest, TiamatBecomesTheDragon) {
	publishSkills({20919, 20920});
	runtime::Ptr<Npc> tiamat = place(219360);
	NpcTiamatProbe& ai = install<NpcTiamatProbe>(*tiamat);
	ai.handleBeforeSpawned();
	EXPECT_EQ(tiamat->getType(player()), model::CreatureType::PEACE);
	ai.onEndUseSkill(skill(20920), 1);
	EXPECT_TRUE(tiamat->isSpawned());
	ai.onEndUseSkill(skill(20919), 1);
	EXPECT_FALSE(tiamat->isSpawned());
	ASSERT_EQ(count({219361}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 219361).front()->getX(), 466.7468f);
}

/** IDTiamat_2_KahrunTalkAI.java:26-55: without an orb his dialog opens; the choice, once, leaves the orb (static 4) 3.5 s later and he goes */
TEST_F(DragonLordsRefugeAiTest, KahrunLeavesTheOrb) {
	runtime::Ptr<Npc> kahrun = place(900200);
	KahrunProbe& ai = install<KahrunProbe>(*kahrun);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(kahrun->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(kahrun->getObjectId(), 0))), 1);
	EXPECT_EQ(kahrun->getType(player()), model::CreatureType::PEACE);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	executor().advance(std::chrono::milliseconds(3499));
	EXPECT_TRUE(kahrun->isSpawned());
	EXPECT_EQ(count({730625}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(kahrun->isSpawned());
	ASSERT_EQ(count({730625}), 1u) << "once";
	EXPECT_EQ(npcs(*instance, 730625).front()->getSpawn()->getStaticId(), 4);
	runtime::Ptr<Npc> again = place(900300);
	install<KahrunProbe>(*again).handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(again->getObjectId(), 1011))), 0) << "the orb is out";
}

/** CalindiSummonsAI.java:24-41: a flame (283133) comes with its texture (283132); both go after 15 s */
TEST_F(DragonLordsRefugeAiTest, TheFlamesGoWithTheirTexture) {
	publishSkills({20914});
	runtime::Ptr<Npc> flame = place(283133);
	install<Probe<ais::CalindiSummonsAI>>(*flame).handleSpawned();
	ASSERT_EQ(count({283132}), 1u);
	EXPECT_EQ(count({283130}), 0u);
	executor().advance(std::chrono::milliseconds(14999));
	EXPECT_TRUE(flame->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(flame->isSpawned());
	EXPECT_EQ(count({283132}), 0u);
}

/** CalindiSummonsAI.java:43-74: a blaze (283131) has the other texture (283130) */
TEST_F(DragonLordsRefugeAiTest, TheBlazesHaveTheirTexture) {
	publishSkills({20916});
	runtime::Ptr<Npc> blaze = place(283131);
	install<Probe<ais::CalindiSummonsAI>>(*blaze).handleSpawned();
	EXPECT_EQ(count({283130}), 1u);
	EXPECT_EQ(count({283132}), 0u);
}

/** UltimateAtrocityAI.java:24-49: fire, a quarter of its damage; with its skill it goes after 11 s */
TEST_F(DragonLordsRefugeAiTest, TheUltimateAtrocitiesGo) {
	publishSkills({21156});
	runtime::Ptr<Npc> atrocity = place(283240);
	auto& ai = install<Probe<ais::UltimateAtrocityAI>>(*atrocity);
	EXPECT_EQ(ai.modifyAttackType(ItemAttackType::PHYSICAL), ItemAttackType::MAGICAL_FIRE);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 25.0f);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(10999));
	EXPECT_TRUE(atrocity->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(atrocity->isSpawned());
}

/** CalculatedAtrocityAI.java:23-58: it never thinks; fire; no damage taken, 70 % of its damage; it goes after 11 s */
TEST_F(DragonLordsRefugeAiTest, TheCalculatedAtrocitiesGo) {
	publishSkills({21894});
	runtime::Ptr<Npc> atrocity = place(900200);
	auto& ai = install<Probe<ais::CalculatedAtrocityAI>>(*atrocity);
	EXPECT_FALSE(ai.canThink());
	EXPECT_EQ(ai.modifyAttackType(ItemAttackType::PHYSICAL), ItemAttackType::MAGICAL_FIRE);
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 42.0f, nullptr), 0.0f);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 70.0f);
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(10999));
	EXPECT_TRUE(atrocity->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(atrocity->isSpawned());
}

/** TiamatSkillHelperAI.java:25-44: 1.5 s after the spawn the next id comes at its place; 3 s later it dies */
TEST_F(DragonLordsRefugeAiTest, TheSkillHelpersPassOn) {
	runtime::Ptr<Npc> helper = place(219362, 310.0f, 320.0f);
	install<Probe<ais::TiamatSkillHelperAI>>(*helper).handleSpawned();
	executor().advance(std::chrono::milliseconds(1499));
	EXPECT_EQ(count({219363}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(count({219363}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 219363).front()->getY(), 320.0f);
	executor().advance(std::chrono::milliseconds(2999));
	EXPECT_FALSE(helper->isDead());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(helper->isDead());
}

/** CalindiFlamelordAI.java:22-62: 75 % queues the victory (20911) while no Surkana is out, 12 % 20942; 20911 brings the Surkanas and ten
 * flames, 20913 a blaze on a random target */
TEST_F(DragonLordsRefugeAiTest, CalindisVictoryAndBlaze) {
	publishSkills({20911, 20913});
	runtime::Ptr<Npc> calindi = place(219359);
	auto& ai = install<ais::CalindiFlamelordAI>(*calindi);
	ai.handleHpPhase(75);
	EXPECT_TRUE(queued(*calindi, 20911));
	EXPECT_FALSE(queued(*calindi, 20942));
	ai.handleHpPhase(12);
	EXPECT_TRUE(queued(*calindi, 20942));
	ai.onEndUseSkill(skill(20911), 1);
	EXPECT_EQ(count({730695, 730696}), 2u);
	EXPECT_EQ(count({283133}), 10u);
	calindi->clearQueuedSkills();
	ai.handleHpPhase(50);
	EXPECT_FALSE(queued(*calindi, 20911)) << "the Surkanas are out";
	npcs(*instance, 730696).front()->getController().delete_();
	ai.handleHpPhase(25);
	EXPECT_FALSE(queued(*calindi, 20911)) << "730695 is out";
	npcs(*instance, 730695).front()->getController().delete_();
	runtime::Ptr<Npc> other = place(730696);
	ai.handleHpPhase(75);
	EXPECT_FALSE(queued(*calindi, 20911)) << "730696 is out";
	other->getController().delete_();
	ai.handleHpPhase(75);
	EXPECT_TRUE(queued(*calindi, 20911)) << "none is out";
	player().getPosition()->setXYZH(305.0f, 320.0f, 300.0f, int8_t{0});
	hit(*calindi);
	ai.onEndUseSkill(skill(20913), 1);
	ASSERT_EQ(count({283131}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 283131).front()->getX(), 305.0f);
	EXPECT_FLOAT_EQ(npcs(*instance, 283131).front()->getY(), 320.0f);
}

/** HM_CalindiFlamelordAI.java:20-36: the anguished victory (21887) and her own Surkanas and flames */
TEST_F(DragonLordsRefugeAiTest, TheAnguishedCalindisVictory) {
	publishSkills({21887, 21888});
	runtime::Ptr<Npc> calindi = place(236274);
	auto& ai = install<ais::HM_CalindiFlamelordAI>(*calindi);
	ai.handleHpPhase(25);
	EXPECT_TRUE(queued(*calindi, 21887));
	EXPECT_FALSE(queued(*calindi, 20911));
	ai.onEndUseSkill(skill(21887), 1);
	EXPECT_EQ(count({731629, 731630}), 2u);
	EXPECT_EQ(count({856298}), 10u);
	EXPECT_EQ(count({730695, 730696, 283133}), 0u);
	hit(*calindi);
	ai.onEndUseSkill(skill(21888), 1);
	EXPECT_EQ(count({856299}), 1u);
}

/** TiamatDragonAI.java:35-49: 20984 warns the map */
TEST_F(DragonLordsRefugeAiTest, TheDragonWarns) {
	publishSkills({20984, 20918});
	auto& ai = install<ais::TiamatDragonAI>(*place(219361));
	ai.onEndUseSkill(skill(20918), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_IDTIAMAT_TIAMAT_WARNING_MSG()), 0);
	ai.onEndUseSkill(skill(20984), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_IDTIAMAT_TIAMAT_WARNING_MSG()), 1);
}

/** GravityCrusherAI.java:41-51, 54-57: its transformation brings its other form (not level 65: the previous id) and it goes; arrived, 20987 */
TEST_F(DragonLordsRefugeAiTest, TheCrushersTransform) {
	publishSkills({20967});
	runtime::Ptr<Npc> crusher = place(283142, 310.0f, 320.0f);
	auto& ai = install<ais::GravityCrusherAI>(*crusher);
	ai.handleMoveArrived();
	EXPECT_TRUE(queued(*crusher, 20987));
	ai.onEndUseSkill(skill(20967), 1);
	EXPECT_FALSE(crusher->isSpawned());
	ASSERT_EQ(count({283141}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 283141).front()->getX(), 310.0f);
}

/** TiamatsIncarnationAI.java:55-60: gone, it takes its summons (Graviwing: 282727, 282729) */
TEST_F(DragonLordsRefugeAiTest, TheIncarnationsTakeTheirSummons) {
	runtime::Ptr<Npc> graviwing = place(219366);
	IncarnationProbe& ai = install<IncarnationProbe>(*graviwing);
	place(282727);
	place(282729);
	runtime::Ptr<Npc> other = place(282731);
	ai.handleDespawned();
	EXPECT_EQ(count({282727, 282729}), 0u);
	EXPECT_TRUE(other->isSpawned());
}

/** EmpyreanLordAI.java:27-41, 76-86: 9 s after the spawn the drakan die; the first phase tells its 50 % hp, the second phase does not */
TEST_F(DragonLordsRefugeAiTest, TheLordsTakeTheDrakan) {
	publishSkills({20932, 20929});
	place(219361);
	runtime::Ptr<Npc> drakan = place(219535);
	install<::aion::gameserver::handlers::ai::GeneralNpcAI>(*drakan);
	runtime::Ptr<Npc> kaisinel = place(219488);
	auto& ai = install<Probe<ais::EmpyreanLordAI>>(*kaisinel);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(8999));
	EXPECT_FALSE(drakan->isDead());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(drakan->isDead());
	ai.handleHpPhase(50);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_GOD_HP_LOWER_THAN_50p()), 1);
	ai.handleHpPhase(15);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_GOD_HP_LOWER_THAN_15p()), 1);
	auto& second = install<Probe<ais::EmpyreanLordAI>>(*place(219489));
	second.handleHpPhase(50);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_GOD_HP_LOWER_THAN_50p()), 1);
}

/** TiamatWeakenedDragonAI.java:42-56, 75-100, 152-167: the atrocity's start brings its fields; 25, 15 and 5 % the gravity crushers; home
 * takes the adds */
TEST_F(DragonLordsRefugeAiTest, TheWeakenedTiamatsAdds) {
	publishSkills({20924, 20926});
	runtime::Ptr<Npc> tiamat = place(219362);
	WeakenedProbe& ai = install<WeakenedProbe>(*tiamat);
	ai.onStartUseSkill(skill(20924), 1);
	EXPECT_EQ(count({283240}), 6u);
	EXPECT_EQ(count({283241}), 5u);
	ai.onStartUseSkill(skill(20926), 1);
	EXPECT_EQ(count({283244}), 1u);
	ai.handleHpPhase(15);
	EXPECT_EQ(count({283141}), 4u);
	ai.handleHpPhase(5);
	EXPECT_EQ(count({283141}), 8u);
	runtime::Ptr<Npc> creation = place(283139);
	runtime::Ptr<Npc> blackhole = place(283140);
	ai.handleBackHome();
	EXPECT_EQ(count({283141, 283139, 283140}), 0u);
	EXPECT_EQ(count({283240}), 6u) << "not an add";
}

/** TiamatWeakenedDragonAI.java:42-48, 109-121: from the first hit, at half hp the divisive creations come 60, 50 and 40 s apart */
TEST_F(DragonLordsRefugeAiTest, TheWeakenedTiamatsCreations) {
	publishSkills({20921, 20922, 20924, 20926});
	runtime::Ptr<Npc> tiamat = place(219362);
	WeakenedProbe& ai = install<WeakenedProbe>(*tiamat);
	hit(*tiamat);
	tiamat->getLifeStats()->setCurrentHpPercent(50);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(59999));
	EXPECT_EQ(count({283139}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({283139}), 4u);
	executor().advance(std::chrono::milliseconds(50000));
	EXPECT_EQ(count({283139}), 8u);
	executor().advance(std::chrono::milliseconds(40000));
	EXPECT_EQ(count({283139}), 12u);
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(count({283139}), 12u) << "three times";
}

/** HM_TiamatWeakenedDragonAI.java:44-57, 116-150: the atrocity's field faces its pattern; the tornadoes take the six spots, two per phase */
TEST_F(DragonLordsRefugeAiTest, TheAnguishedTiamatsTornadoes) {
	publishSkills({20924, 20922});
	commons::utils::Rnd::seedCurrentThreadForTests(20261018);
	runtime::Ptr<Npc> tiamat = place(236277);
	HmWeakenedProbe& ai = install<HmWeakenedProbe>(*tiamat);
	ai.handleSpawned();
	ai.onStartUseSkill(skill(20922), 1);
	ASSERT_EQ(count({283238}), 1u);
	EXPECT_EQ(npcs(*instance, 283238).front()->getHeading(), 25);
	ai.handleHpPhase(20);
	EXPECT_EQ(count({856046}), 2u);
	ai.handleHpPhase(15);
	ai.handleHpPhase(5);
	ai.handleHpPhase(25);
	ai.handleHpPhase(5);
	ai.handleHpPhase(5);
	EXPECT_EQ(count({856046}), 12u);
	ai.handleHpPhase(5);
	EXPECT_EQ(count({856046}), 12u) << "the six spots are taken";
	for (const runtime::Ptr<Npc>& tornado : npcs(*instance, 856046)) // the spots are 7 to 42 m away
		EXPECT_LT(utils::PositionUtil::getDistance(tornado->getX(), tornado->getY(), tiamat->getX(), tiamat->getY()), 42.5);
	ai.handleBackHome();
	EXPECT_EQ(count({856046}), 0u);
	ai.handleHpPhase(20);
	EXPECT_EQ(count({856046}), 2u) << "home resets the spots";
	EXPECT_EQ(count({283141}), 0u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
