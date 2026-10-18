// P6-I2 the Illuminary Obelisk's AIs (game-server/data/handlers/ai/instance/illuminaryObelisk) on npcs of real Illuminary Obelisk instances,
// each AI installed on its npc and driven through its protected events. Skills are plain templates; a probe records the skills its npc
// starts. The test world sends an attacked npc home at once (no fight here): the probes go home only when a case asks. Left out: the walks
// of the vortices' walkers (no walker routes), the abort of a charge by the player's action (the observer's trigger).

#include "I2TestSupport.h"
#include "CrucibleTestSupport.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/DynatoumAI.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/DynatoumHealerAI.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/DynatoumMaintenanceDeviceAI.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/IDF5_U3_VortexAI.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/InfernalDynatoumAI.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorEastAI.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorNorthAI.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorSouthAI.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorWestAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {
::aion::gameserver::handlers::AIFactory DynatoumAI_aiFactory;
::aion::gameserver::handlers::AIFactory DynatoumHealerAI_aiFactory;
::aion::gameserver::handlers::AIFactory DynatoumMaintenanceDeviceAI_aiFactory;
::aion::gameserver::handlers::AIFactory IDF5_U3_VortexAI_aiFactory;
::aion::gameserver::handlers::AIFactory InfernalDynatoumAI_aiFactory;
::aion::gameserver::handlers::AIFactory ShieldGeneratorEastAI_aiFactory;
::aion::gameserver::handlers::AIFactory ShieldGeneratorNorthAI_aiFactory;
::aion::gameserver::handlers::AIFactory ShieldGeneratorSouthAI_aiFactory;
::aion::gameserver::handlers::AIFactory ShieldGeneratorWestAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::illuminaryObelisk;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

using ObeliskCasts = std::vector<std::pair<int32_t, int32_t>>;

template <class AI>
struct ObeliskProbe : AI {
	using AI::AI;
	using AI::handleSpawned;
	using AI::handleDied;
	using AI::handleDespawned;
	ObeliskCasts casts;
	bool homeAllowed = false;
	void handleBackHome() override {
		if (homeAllowed)
			AI::handleBackHome();
	}
	void onStartUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override {
		casts.emplace_back(skillTemplate->getSkillId(), skillLevel);
		AI::onStartUseSkill(skillTemplate, skillLevel);
	}
	void attackedBy(model::gameobjects::Creature& creature) { AI::handleAttack(runtime::Ptr<model::gameobjects::Creature>(creature)); }
};
template <class AI>
struct GeneratorProbe : ObeliskProbe<AI> {
	using ObeliskProbe<AI>::ObeliskProbe;
	using AI::handleDialogStart;
	int32_t charges() { return this->chargeCount.get(); }
};

class IlluminaryObeliskAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		publishCrucibleData(context);
	}

	void TearDown() override {
		InstanceHandlerTest::TearDown();
		resetCrucibleData();
	}

	void open(int32_t mapId = ILLUMINARY_OBELISK) {
		instance = &create(mapId, mapId == ILLUMINARY_OBELISK ? IlluminaryObeliskInstance_instanceFactory : InfernalIlluminaryObelisk_instanceFactory);
		enter(*instance, 255.0f, 297.0f, 321.4f);
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

	runtime::Ptr<Npc> place(int32_t npcId, float dy = 1.0f) {
		runtime::Ptr<Npc> npc = spawnNpc(*instance, npcId, 255.0f, 297.0f + dy, 321.4f, int8_t{0});
		InstanceKnownListPairing::pair(*npc, player());
		return npc;
	}

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="obelisk" nameId="1" stack="OBELISK" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="ALL" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	static const skillengine::model::SkillTemplate* skill(int32_t id) { return dataholders::DataManager::SKILL_DATA->getSkillTemplate(id); }

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	int64_t said(SM_SYSTEM_MESSAGE&& msg) { return countSent(serialized(std::move(msg))); }

	static int32_t queued(Npc& npc) {
		runtime::Ptr<model::skill::NpcSkillEntry> next = npc.getNextQueuedSkill();
		return next == nullptr ? 0 : next->getSkillId();
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

/** The nine AION_AI factories build their classes */
TEST_F(IlluminaryObeliskAiTest, TheFactoriesBuildTheirClasses) {
	open();
	runtime::Ptr<Npc> npc = place(233740);
	EXPECT_NE(dynamic_cast<ais::DynatoumAI*>(ais::DynatoumAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DynatoumHealerAI*>(ais::DynatoumHealerAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DynatoumMaintenanceDeviceAI*>(ais::DynatoumMaintenanceDeviceAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::IDF5_U3_VortexAI*>(ais::IDF5_U3_VortexAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::InfernalDynatoumAI*>(ais::InfernalDynatoumAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ShieldGeneratorEastAI*>(ais::ShieldGeneratorEastAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ShieldGeneratorNorthAI*>(ais::ShieldGeneratorNorthAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ShieldGeneratorSouthAI*>(ais::ShieldGeneratorSouthAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ShieldGeneratorWestAI*>(ais::ShieldGeneratorWestAI_aiFactory(*npc).get()), nullptr);
}

/** DynatoumAI.java: the ten minutes from the first attack, the portals at 70 %, the self-destruction's end */
TEST_F(IlluminaryObeliskAiTest, Dynatoum) {
	publishSkills({21534, 21535});
	open();
	runtime::Ptr<Npc> boss = place(233740);
	ObeliskProbe<ais::DynatoumAI>& ai = install<ObeliskProbe<ais::DynatoumAI>>(*boss);
	runtime::Ptr<Npc> portal = place(702216, 3.0f);
	ai.attackedBy(player());
	after(999);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_01()), 0);
	after(1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_01()), 1);
	ai.attackedBy(player());
	after(299999);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_02()), 0);
	after(1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_02()), 1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_01()), 1) << "one timer";
	after(240000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_03()), 1);
	EXPECT_EQ(queued(*boss), 0);
	after(60000);
	EXPECT_EQ(queued(*boss), 21534) << "the self-destruction is queued at the end";
	EXPECT_TRUE(portal->isSpawned());
	boss->getLifeStats()->setCurrentHpPercent(70);
	ai.attackedBy(player());
	EXPECT_FALSE(portal->isSpawned());
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_PORTAL_DESTROY()), 1);
	ai.onEndUseSkill(skill(21535), 1);
	EXPECT_TRUE(boss->isSpawned());
	ai.onEndUseSkill(skill(21534), 1);
	EXPECT_FALSE(boss->isSpawned());
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_04()), 1);
}

/** DynatoumAI.java: a dead Dynatoum's timer is cancelled; above 70 % the portals stay */
TEST_F(IlluminaryObeliskAiTest, DynatoumDies) {
	open();
	runtime::Ptr<Npc> boss = place(233740);
	ObeliskProbe<ais::DynatoumAI>& ai = install<ObeliskProbe<ais::DynatoumAI>>(*boss);
	runtime::Ptr<Npc> portal = place(702216, 3.0f);
	boss->getLifeStats()->setCurrentHpPercent(71);
	ai.attackedBy(player());
	EXPECT_TRUE(portal->isSpawned());
	after(1000);
	ai.handleDied();
	after(600000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_02()), 0);
}

/** InfernalDynatoumAI.java: the hard mode's timer and messages */
TEST_F(IlluminaryObeliskAiTest, InfernalDynatoum) {
	publishSkills({21534});
	open(INFERNAL_ILLUMINARY_OBELISK);
	runtime::Ptr<Npc> boss = place(234686);
	ObeliskProbe<ais::InfernalDynatoumAI>& ai = install<ObeliskProbe<ais::InfernalDynatoumAI>>(*boss);
	runtime::Ptr<Npc> portal = place(702216, 3.0f);
	ai.attackedBy(player());
	after(1000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_01()), 1);
	after(1999);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_02()), 0);
	after(1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_02()), 1);
	after(240000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_03()), 1);
	after(60000);
	EXPECT_EQ(queued(*boss), 21534);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_BOSS_TIMER_01()), 0) << "not the normal messages";
	boss->getLifeStats()->setCurrentHpPercent(60);
	ai.attackedBy(player());
	EXPECT_FALSE(portal->isSpawned());
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_PORTAL_DESTROY()), 1);
	ai.onEndUseSkill(skill(21534), 1);
	EXPECT_FALSE(boss->isSpawned());
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_HARD_BOSS_TIMER_04()), 1);
}

/** DynatoumHealerAI.java: no thinking; the healer targets Dynatoum and queues its heal, again after each */
TEST_F(IlluminaryObeliskAiTest, TheHealer) {
	publishSkills({21535, 21536});
	open();
	runtime::Ptr<Npc> boss = place(233740);
	runtime::Ptr<Npc> healer = place(233881, 3.0f);
	ObeliskProbe<ais::DynatoumHealerAI>& ai = install<ObeliskProbe<ais::DynatoumHealerAI>>(*healer);
	EXPECT_FALSE(ai.canThink());
	ai.handleSpawned();
	EXPECT_EQ(healer->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(boss));
	EXPECT_EQ(queued(*healer), 21535);
	healer->clearQueuedSkills();
	ai.onEndUseSkill(skill(21536), 1);
	EXPECT_EQ(queued(*healer), 0);
	ai.onEndUseSkill(skill(21535), 1);
	EXPECT_EQ(queued(*healer), 21535) << "the next heal";
}

/** :25-31: without Dynatoum the healer does nothing; in the infernal obelisk it heals Infernal Dynatoum */
TEST_F(IlluminaryObeliskAiTest, TheHealerFindsItsBoss) {
	open(INFERNAL_ILLUMINARY_OBELISK);
	place(233740);
	runtime::Ptr<Npc> healer = place(233881, 3.0f);
	install<ObeliskProbe<ais::DynatoumHealerAI>>(*healer).handleSpawned();
	EXPECT_EQ(queued(*healer), 0) << "233740 is not this map's boss";
	runtime::Ptr<Npc> boss = place(234686, 5.0f);
	runtime::Ptr<Npc> second = place(233881, 4.0f);
	install<ObeliskProbe<ais::DynatoumHealerAI>>(*second).handleSpawned();
	EXPECT_EQ(second->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(boss));
}

/** DynatoumMaintenanceDeviceAI.java: no thinking, no decay, respawn or rewards; a heal every 10 s after the last ended */
TEST_F(IlluminaryObeliskAiTest, TheMaintenanceDevice) {
	publishSkills({21535, 21536});
	open();
	runtime::Ptr<Npc> boss = place(233740);
	runtime::Ptr<Npc> device = place(702216, 3.0f);
	ObeliskProbe<ais::DynatoumMaintenanceDeviceAI>& ai = install<ObeliskProbe<ais::DynatoumMaintenanceDeviceAI>>(*device);
	EXPECT_FALSE(ai.canThink());
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	ai.handleSpawned();
	EXPECT_EQ(device->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(boss));
	after(9999);
	EXPECT_TRUE(ai.casts.empty());
	after(1);
	EXPECT_EQ(ai.casts, (ObeliskCasts{{21535, 1}}));
	after(10000);
	EXPECT_EQ(ai.casts.size(), 2u) << "the heal's end schedules the next";
	ai.handleDespawned();
	after(20000);
	EXPECT_EQ(ai.casts.size(), 2u) << "cancelled";
}

/** IDF5_U3_VortexAI.java: every two minutes two waves of walkers of its mode's ids; gone with the vortex */
TEST_F(IlluminaryObeliskAiTest, TheVortex) {
	open();
	runtime::Ptr<Npc> vortex = place(702014, 40.0f);
	ObeliskProbe<ais::IDF5_U3_VortexAI>& ai = install<ObeliskProbe<ais::IDF5_U3_VortexAI>>(*vortex);
	ai.handleSpawned();
	after(119999);
	EXPECT_EQ(count(233857), 0u);
	after(1);
	EXPECT_EQ(count(233857), 2u);
	EXPECT_EQ(count(233880), 4u);
	after(15000);
	EXPECT_EQ(count(233857), 4u);
	EXPECT_EQ(count(233881), 4u);
	for (const runtime::Ptr<Npc>& walker : npcs(*instance, 233881)) {
		const std::string route = walker->getSpawn()->getWalkerId().value_or("");
		EXPECT_TRUE(route == "idf5_u3_east_3" || route == "idf5_u3_east_4" || route == "idf5_u3_east_5" || route == "idf5_u3_east_6") << route;
	}
	ai.handleDespawned();
	for (const runtime::FutureRef& task : executor().pendingTasks())
		EXPECT_EQ(std::string(task->getTaskInfo().where.file_name()).find("IDF5_U3_VortexAI"), std::string::npos) << "the vortex's tasks are cancelled";
	after(240000);
	EXPECT_EQ(count(233857), 4u) << "no more waves";
}

/** :28-34: the hard mode's walkers, from each side */
TEST_F(IlluminaryObeliskAiTest, TheInfernalVortices) {
	open(INFERNAL_ILLUMINARY_OBELISK);
	for (int32_t id : {702015, 702016, 702017})
		install<ObeliskProbe<ais::IDF5_U3_VortexAI>>(*place(id, 40.0f)).handleSpawned();
	after(120000);
	EXPECT_EQ(count(234687), 6u);
	EXPECT_EQ(count(234688), 12u);
	after(15000);
	EXPECT_EQ(count(234689), 12u);
	EXPECT_EQ(count(233857), 0u);
}

/** ShieldGeneratorAI.java: the dialog; no battery, no charge; a charge of 20 s takes a battery and adds a charge */
TEST_F(IlluminaryObeliskAiTest, AGeneratorCharge) {
	open();
	runtime::Ptr<Npc> generator = place(702010);
	GeneratorProbe<ais::ShieldGeneratorEastAI>& ai = install<GeneratorProbe<ais::ShieldGeneratorEastAI>>(*generator);
	EXPECT_FALSE(ai.canThink());
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(generator->getObjectId(), 1011))), 1);
	EXPECT_FALSE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_NOITEM()), 1);
	EXPECT_EQ(count(702014), 0u);
	services::item::ItemService::addItem(player(), 164000289, 4);
	(*client)->clearSent();
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(count(702014), 1u) << "the vortex at the first charge";
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_01_BEGIN()), 1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_01()), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(count(702014), 1u) << "one vortex";
	after(19999);
	EXPECT_EQ(ai.charges(), 0);
	after(1);
	EXPECT_EQ(ai.charges(), 1);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(164000289), 3);
	EXPECT_EQ(count(702218), 1u) << "the first charge";
}

/** :80-98, 132-144: three charges at most; the generator's death takes its charges and its vortex */
TEST_F(IlluminaryObeliskAiTest, AGeneratorFull) {
	open();
	runtime::Ptr<Npc> generator = place(702010);
	GeneratorProbe<ais::ShieldGeneratorEastAI>& ai = install<GeneratorProbe<ais::ShieldGeneratorEastAI>>(*generator);
	services::item::ItemService::addItem(player(), 164000289, 4);
	for (int32_t i = 0; i < 3; i++) {
		ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0);
		after(20000);
	}
	EXPECT_EQ(count(702218) + count(702219) + count(702220), 3u);
	(*client)->clearSent();
	EXPECT_FALSE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_END()), 1);
	ai.handleDied();
	EXPECT_EQ(count(702218) + count(702219) + count(702220), 0u);
	EXPECT_EQ(count(702014), 0u);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_DESTROY_01()), 1);
}

/** :62-69: the attack shout, at most every 10 s */
TEST_F(IlluminaryObeliskAiTest, AGeneratorUnderAttack) {
	open();
	runtime::Ptr<Npc> generator = place(702010);
	GeneratorProbe<ais::ShieldGeneratorWestAI>& ai = install<GeneratorProbe<ais::ShieldGeneratorWestAI>>(*generator);
	ai.attackedBy(player());
	ai.attackedBy(player());
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DEFENCE_02_ATTACKED()), 1);
}

/** The other three generators: their charge npcs, vortices and messages */
TEST_F(IlluminaryObeliskAiTest, TheOtherGenerators) {
	open();
	services::item::ItemService::addItem(player(), 164000289, 3);
	GeneratorProbe<ais::ShieldGeneratorWestAI>& west = install<GeneratorProbe<ais::ShieldGeneratorWestAI>>(*place(702010));
	GeneratorProbe<ais::ShieldGeneratorSouthAI>& south = install<GeneratorProbe<ais::ShieldGeneratorSouthAI>>(*place(702011, 2.0f));
	GeneratorProbe<ais::ShieldGeneratorNorthAI>& north = install<GeneratorProbe<ais::ShieldGeneratorNorthAI>>(*place(702012, 3.0f));
	west.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_02()), 1);
	south.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_03()), 1);
	(*client)->clearSent();
	north.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_04_BEGIN()), 1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_04()), 1);
	EXPECT_EQ(count(702015), 1u) << "the west vortex";
	EXPECT_EQ(count(702016), 1u) << "the south vortex";
	EXPECT_EQ(count(702017), 1u) << "the north vortex";
	after(20000);
	EXPECT_EQ(count(702221), 0u) << "one charge task per player (TaskId.ACTION_ITEM_NPC): the last charge replaces the others";
	EXPECT_EQ(count(702224), 0u);
	EXPECT_EQ(count(702227), 1u);
	(*client)->clearSent();
	south.attackedBy(player());
	north.attackedBy(player());
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DEFENCE_03_ATTACKED()), 1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DEFENCE_04_ATTACKED()), 1);
	west.handleDied();
	south.handleDied();
	north.handleDied();
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_DESTROY_02()), 1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_DESTROY_03()), 1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_DESTROY_04()), 1);
	EXPECT_EQ(count(702015) + count(702016) + count(702017), 0u);
	EXPECT_EQ(count(702221) + count(702224) + count(702227), 0u);
}

/** :53-56 (J-I2-3, kept): the west and south generators shout the east's wave message, not their own */
TEST_F(IlluminaryObeliskAiTest, TheWestAndSouthShoutTheEastsWave) {
	open();
	services::item::ItemService::addItem(player(), 164000289, 2);
	install<GeneratorProbe<ais::ShieldGeneratorWestAI>>(*place(702010)).onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0);
	install<GeneratorProbe<ais::ShieldGeneratorSouthAI>>(*place(702011, 2.0f)).onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_01_BEGIN()), 2);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_02_BEGIN()), 0);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_N_WAVE_03_BEGIN()), 0);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
