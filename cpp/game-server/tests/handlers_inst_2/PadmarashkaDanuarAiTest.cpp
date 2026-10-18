// P6-I2 the padmarashkasCave and danuarSanctuary AIs (game-server/data/handlers/ai/instance/{padmarashkasCave,danuarSanctuary}) on npcs of
// real instances, each AI installed on its npc and driven through its protected events. Skills are plain templates; a probe records the
// skills its npc starts. The test world sends an attacked npc home at once (no fight here): the probes go home only when a case asks. Left
// out: the siege drill's walk (no walker route), the start portals' teleports, the protectors' and hatchlings' walk to their target.

#include "I2TestSupport.h"
#include "CrucibleTestSupport.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/danuarSanctuary/AncientDanuarCoffinAI.h"
#include "aion/gameserver/handlers/ai/instance/danuarSanctuary/SiegeDrill.h"
#include "aion/gameserver/handlers/ai/instance/danuarSanctuary/StartTeleportAI.h"
#include "aion/gameserver/handlers/ai/instance/padmarashkasCave/PadmarashkaAI.h"
#include "aion/gameserver/handlers/ai/instance/padmarashkasCave/PadmarashkaEggAI.h"
#include "aion/gameserver/handlers/ai/instance/padmarashkasCave/RockSlideAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::padmarashkasCave {
::aion::gameserver::handlers::AIFactory PadmarashkaAI_aiFactory;
::aion::gameserver::handlers::AIFactory PadmarashkaEggAI_aiFactory;
::aion::gameserver::handlers::AIFactory RockSlideAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::padmarashkasCave
namespace aion::gameserver::handlers::ai::instance::danuarSanctuary {
::aion::gameserver::handlers::AIFactory AncientDanuarCoffinAI_aiFactory;
::aion::gameserver::handlers::AIFactory SiegeDrill_aiFactory;
::aion::gameserver::handlers::AIFactory StartTeleportAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::danuarSanctuary

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace pad = ::aion::gameserver::handlers::ai::instance::padmarashkasCave;
namespace dan = ::aion::gameserver::handlers::ai::instance::danuarSanctuary;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

template <class AI>
struct CaveProbe : AI {
	using AI::AI;
	using AI::handleSpawned;
	using AI::handleDied;
	using AI::handleDespawned;
	std::vector<int32_t> casts;
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
		casts.push_back(skillTemplate->getSkillId());
		AI::onStartUseSkill(skillTemplate, skillLevel);
	}
	void attackedBy(model::gameobjects::Creature& creature) { AI::handleAttack(runtime::Ptr<model::gameobjects::Creature>(creature)); }
};
template <class AI>
struct DialogCaveProbe : CaveProbe<AI> {
	using CaveProbe<AI>::CaveProbe;
	using AI::handleDialogStart;
};
struct Recorder : ::aion::gameserver::handlers::ai::GeneralNpcAI {
	using GeneralNpcAI::GeneralNpcAI;
	std::vector<int32_t> casts;
	void onStartUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override {
		casts.push_back(skillTemplate->getSkillId());
		GeneralNpcAI::onStartUseSkill(skillTemplate, skillLevel);
	}
};

class PadmarashkaDanuarAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		publishCrucibleData(context);
	}

	void TearDown() override {
		InstanceHandlerTest::TearDown();
		resetCrucibleData();
	}

	void open(int32_t mapId) {
		::aion::gameserver::handlers::InstanceFactory& factory = mapId == PADMARASHKAS_CAVE ? PadmarashkasCaveInstance_instanceFactory
			: mapId == DANUAR_SANCTUARY                                                       ? DanuarSanctuaryInstance_instanceFactory
																							  : SeizedDanuarSanctuaryInstance_instanceFactory;
		instance = &create(mapId, factory);
		enter(*instance, 560.0f, 220.0f, 66.5f);
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
		runtime::Ptr<Npc> npc = spawnNpc(*instance, npcId, 560.0f, 220.0f + dy, 66.5f, int8_t{0});
		InstanceKnownListPairing::pair(*npc, player());
		return npc;
	}

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="cave" nameId="1" stack="CAVE" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="ALL" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	template <class AI>
	void attackedAt(Npc& npc, AI& ai, int32_t hpPercent) {
		npc.getLifeStats()->setCurrentHpPercent(hpPercent);
		ai.attackedBy(player());
	}

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	size_t countRange(int32_t from, int32_t to) {
		size_t n = 0;
		for (int32_t id = from; id <= to; id++)
			n += count(id);
		return n;
	}

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	static void seedFor(const std::function<bool()>& draws) {
		for (uint64_t seed = 1; seed < 10000; seed++) {
			commons::utils::Rnd::seedCurrentThreadForTests(seed);
			if (draws()) {
				commons::utils::Rnd::seedCurrentThreadForTests(seed);
				return;
			}
		}
		FAIL() << "no seed";
	}

	int64_t dialogs(Npc& npc, int32_t page) { return countSent(serialized(SM_DIALOG_WINDOW(npc.getObjectId(), page))); }

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

/** The six AION_AI factories build their classes */
TEST_F(PadmarashkaDanuarAiTest, TheFactoriesBuildTheirClasses) {
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> npc = place(218756);
	EXPECT_NE(dynamic_cast<pad::PadmarashkaAI*>(pad::PadmarashkaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<pad::PadmarashkaEggAI*>(pad::PadmarashkaEggAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<pad::RockSlideAI*>(pad::RockSlideAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<dan::AncientDanuarCoffinAI*>(dan::AncientDanuarCoffinAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<dan::SiegeDrill*>(dan::SiegeDrill_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<dan::StartTeleportAI*>(dan::StartTeleportAI_aiFactory(*npc).get()), nullptr);
}

/** PadmarashkaAI.java: asleep at her spawn; the first attack wakes her: the entrance adds and her main skill every 20 s */
TEST_F(PadmarashkaDanuarAiTest, PadmarashkaWakesUp) {
	publishSkills({19186, 20401, 20093});
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> padma = place(218756);
	CaveProbe<pad::PadmarashkaAI>& ai = install<CaveProbe<pad::PadmarashkaAI>>(*padma);
	ai.handleSpawned();
	EXPECT_EQ(ai.casts, (std::vector<int32_t>{19186})) << "put to sleep";
	EXPECT_TRUE(padma->getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::SLEEP));
	EXPECT_FALSE(ai.canThink());
	attackedAt(*padma, ai, 100);
	EXPECT_TRUE(ai.canThink());
	const size_t adds = countRange(282792, 282796);
	EXPECT_GE(adds, 1u);
	EXPECT_LE(adds, 3u);
	after(10000);
	ASSERT_EQ(ai.casts.size(), 2u);
	EXPECT_TRUE(ai.casts[1] == 20401 || ai.casts[1] == 20093);
	after(20000);
	EXPECT_EQ(ai.casts.size(), 3u) << "every 20 s";
	after(30000);
	EXPECT_GT(countRange(282792, 282796), adds) << "the entrance again a minute later";
}

/** :78-84 and the stages: 95 % an egg and a protector, 50 % a huge egg, 25 % the rocks */
TEST_F(PadmarashkaDanuarAiTest, PadmarashkasPhases) {
	publishSkills({19186, 20401, 20093, 19177, 19179});
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> padma = place(218756);
	CaveProbe<pad::PadmarashkaAI>& ai = install<CaveProbe<pad::PadmarashkaAI>>(*padma);
	attackedAt(*padma, ai, 95);
	EXPECT_EQ(std::count(ai.casts.begin(), ai.casts.end(), 19177), 1);
	EXPECT_EQ(count(282613), 1u);
	EXPECT_EQ(count(218675), 1u);
	after(60000);
	EXPECT_EQ(count(282613), 2u) << "another egg a minute later";
	EXPECT_EQ(count(218676), 1u) << "the second protector";
	(*client)->clearSent();
	attackedAt(*padma, ai, 50);
	EXPECT_EQ(count(282614), 1u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1401214, std::vector<std::string>{}))), 1);
	attackedAt(*padma, ai, 25);
	EXPECT_EQ(std::count(ai.casts.begin(), ai.casts.end(), 19179), 1);
	EXPECT_EQ(count(282140), 18u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1401215, std::vector<std::string>{}))), 1);
	after(120000);
	EXPECT_EQ(count(282614), 2u) << "another huge egg two minutes later";
}

/** :258-268: home: everything goes, she sleeps again and her four protectors come back */
TEST_F(PadmarashkaDanuarAiTest, PadmarashkaGoesHome) {
	publishSkills({19186, 20401, 20093, 19177, 19179});
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> padma = place(218756);
	CaveProbe<pad::PadmarashkaAI>& ai = install<CaveProbe<pad::PadmarashkaAI>>(*padma);
	attackedAt(*padma, ai, 95);
	attackedAt(*padma, ai, 25);
	ai.goHome();
	EXPECT_EQ(count(282140) + count(282613) + count(218675) + countRange(282792, 282796), 0u);
	EXPECT_EQ(count(218670) + count(218671) + count(218673) + count(218674), 4u);
	EXPECT_FALSE(ai.canThink());
	const size_t casts = ai.casts.size();
	after(60000);
	EXPECT_EQ(ai.casts.size(), casts) << "the main skill task is cancelled, the stages stop";
	EXPECT_EQ(count(282613), 0u);
	attackedAt(*padma, ai, 94);
	EXPECT_EQ(count(282613), 1u) << "the phases start again";
}

/** :270-283: her death and despawn take the adds */
TEST_F(PadmarashkaDanuarAiTest, PadmarashkaDies) {
	publishSkills({19186, 20401, 20093, 19177});
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> padma = place(218756);
	CaveProbe<pad::PadmarashkaAI>& ai = install<CaveProbe<pad::PadmarashkaAI>>(*padma);
	attackedAt(*padma, ai, 95);
	place(218670);
	ai.handleDied();
	EXPECT_EQ(count(282613) + count(218675) + count(218670) + countRange(282792, 282796), 0u);
	const size_t casts = ai.casts.size();
	after(60000);
	EXPECT_EQ(ai.casts.size(), casts);
	EXPECT_EQ(count(282613), 0u) << "dead: no more stages";
}

/** PadmarashkaEggAI.java: a small egg's protector at the first attack only; its death buffs it; unbroken it hatches after a minute */
TEST_F(PadmarashkaDanuarAiTest, ASmallEgg) {
	publishSkills({20176});
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> egg = place(282613);
	CaveProbe<pad::PadmarashkaEggAI>& ai = install<CaveProbe<pad::PadmarashkaEggAI>>(*egg);
	ai.handleSpawned();
	seedFor([] { return commons::utils::Rnd::get(1, 6) == 5; });
	ai.attackedBy(player());
	ASSERT_EQ(count(282716), 1u);
	ai.attackedBy(player());
	EXPECT_EQ(count(282715) + count(282716), 1u) << "one protector";
	runtime::Ptr<Npc> protector = npcs(*instance, 282716)[0];
	Recorder& recorder = install<Recorder>(*protector);
	ai.handleDied();
	EXPECT_EQ(recorder.casts, (std::vector<int32_t>{20176})) << "the protector's wrath";
}

/** :82-89: an unbroken small egg hatches after a minute */
TEST_F(PadmarashkaDanuarAiTest, ASmallEggHatches) {
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> egg = place(282613);
	install<CaveProbe<pad::PadmarashkaEggAI>>(*egg).handleSpawned();
	after(59999);
	EXPECT_TRUE(egg->isSpawned());
	after(1);
	EXPECT_FALSE(egg->isSpawned());
	EXPECT_EQ(count(282616), 1u);
}

/** :55-60, 90-97: a huge egg calls the elite commander; unbroken it hatches after two minutes */
TEST_F(PadmarashkaDanuarAiTest, AHugeEgg) {
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> egg = place(282614);
	CaveProbe<pad::PadmarashkaEggAI>& ai = install<CaveProbe<pad::PadmarashkaEggAI>>(*egg);
	ai.handleSpawned();
	ai.attackedBy(player());
	ai.attackedBy(player());
	EXPECT_EQ(count(282712), 1u);
	EXPECT_EQ(count(282715) + count(282716), 0u);
	after(119999);
	EXPECT_TRUE(egg->isSpawned());
	after(1);
	EXPECT_FALSE(egg->isSpawned());
	EXPECT_EQ(count(282620), 1u);
}

/** RockSlideAI.java: no thinking, no damage; the rock slide 2 s after its spawn and 2.7 s after each */
TEST_F(PadmarashkaDanuarAiTest, ARockSlide) {
	publishSkills({19295});
	open(PADMARASHKAS_CAVE);
	runtime::Ptr<Npc> rock = place(282140);
	CaveProbe<pad::RockSlideAI>& ai = install<CaveProbe<pad::RockSlideAI>>(*rock);
	EXPECT_FALSE(ai.canThink());
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 500.0f, nullptr), 0.0f);
	ai.handleSpawned();
	after(1999);
	EXPECT_TRUE(ai.casts.empty());
	after(1);
	EXPECT_EQ(ai.casts, (std::vector<int32_t>{19295}));
	EXPECT_EQ(rock->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(rock));
	after(2699);
	EXPECT_EQ(ai.casts.size(), 1u);
	after(1);
	EXPECT_EQ(ai.casts.size(), 2u) << "again 2.7 s after the end";
}

/** AncientDanuarCoffinAI.java: no thinking, one damage; a 40 % chance of a mummy at its death */
TEST_F(PadmarashkaDanuarAiTest, TheCoffin) {
	open(DANUAR_SANCTUARY);
	runtime::Ptr<Npc> coffin = place(233085, 3.0f);
	CaveProbe<dan::AncientDanuarCoffinAI>& ai = install<CaveProbe<dan::AncientDanuarCoffinAI>>(*coffin);
	EXPECT_FALSE(ai.canThink());
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 500.0f, nullptr), 1.0f);
	const size_t before = count(233085);
	seedFor([] { return commons::utils::Rnd::chance() >= 40; });
	ai.handleDied();
	EXPECT_EQ(count(233085), before);
	runtime::Ptr<Npc> second = place(233085, 4.0f);
	CaveProbe<dan::AncientDanuarCoffinAI>& secondAi = install<CaveProbe<dan::AncientDanuarCoffinAI>>(*second);
	const size_t now = count(233085);
	seedFor([] { return commons::utils::Rnd::chance() < 40; });
	secondAi.handleDied();
	EXPECT_EQ(count(233085), now + 1) << "a mummy";
}

/** SiegeDrill.java: no damage, attacks do nothing; without a key the map hears 1401932 */
TEST_F(PadmarashkaDanuarAiTest, TheSiegeDrillWithoutAKey) {
	open(SEIZED_DANUAR_SANCTUARY);
	runtime::Ptr<Npc> drill = place(233187, 3.0f);
	DialogCaveProbe<dan::SiegeDrill>& ai = install<DialogCaveProbe<dan::SiegeDrill>>(*drill);
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 500.0f, nullptr), 0.0f);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1401932, std::vector<std::string>{}))), 1);
	EXPECT_FALSE(drill->getSpawn()->getWalkerId().has_value());
}

/** :88-104: with a key it takes the key, drives, and 4.8 s later bores the barricade; 5 s more and it is gone; only once */
TEST_F(PadmarashkaDanuarAiTest, TheSiegeDrillWithAKey) {
	publishSkills({20778});
	open(SEIZED_DANUAR_SANCTUARY);
	runtime::Ptr<Npc> barricade = place(233189, 6.0f);
	runtime::Ptr<Npc> drill = place(233187, 3.0f);
	DialogCaveProbe<dan::SiegeDrill>& ai = install<DialogCaveProbe<dan::SiegeDrill>>(*drill);
	services::item::ItemService::addItem(player(), 185000174, 2);
	ai.handleDialogStart(player());
	EXPECT_EQ(player().getInventory().getItemCountByItemId(185000174), 1);
	EXPECT_EQ(drill->getSpawn()->getWalkerId(), std::optional<std::string>("2A2D7F6EA351DCCEAA3CD097B9311ED308DCD2EC"));
	EXPECT_TRUE(drill->isInState(model::gameobjects::state::CreatureState::ACTIVE));
	ai.handleDialogStart(player());
	EXPECT_EQ(player().getInventory().getItemCountByItemId(185000174), 1) << "used: the dialog does nothing";
	after(4799);
	EXPECT_TRUE(ai.casts.empty());
	after(1);
	EXPECT_EQ(ai.casts, (std::vector<int32_t>{20778}));
	EXPECT_EQ(drill->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(barricade));
	EXPECT_FALSE(drill->getSpawn()->getWalkerId().has_value());
	after(4999);
	EXPECT_TRUE(drill->isSpawned());
	after(1);
	EXPECT_FALSE(drill->isSpawned());
}

/** :96-104: without the barricade it keeps driving */
TEST_F(PadmarashkaDanuarAiTest, TheSiegeDrillWithoutABarricade) {
	publishSkills({20778});
	open(SEIZED_DANUAR_SANCTUARY);
	runtime::Ptr<Npc> drill = place(233187, 3.0f);
	DialogCaveProbe<dan::SiegeDrill>& ai = install<DialogCaveProbe<dan::SiegeDrill>>(*drill);
	services::item::ItemService::addItem(player(), 185000174, 1);
	ai.handleDialogStart(player());
	after(10000);
	EXPECT_TRUE(ai.casts.empty());
	EXPECT_TRUE(drill->isSpawned());
}

/** StartTeleportAI.java: the dialog; without its key the no-right page */
TEST_F(PadmarashkaDanuarAiTest, TheStartPortalWithoutAKey) {
	open(SEIZED_DANUAR_SANCTUARY);
	runtime::Ptr<Npc> portal = place(701871, 3.0f);
	DialogCaveProbe<dan::StartTeleportAI>& ai = install<DialogCaveProbe<dan::StartTeleportAI>>(*portal);
	ai.handleDialogStart(player());
	EXPECT_EQ(dialogs(*portal, 1011), 1);
	services::item::ItemService::addItem(player(), 185000182, 1); // another portal's key
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(dialogs(*portal, model::id(model::DialogPage::NO_RIGHT)), 1);
	EXPECT_EQ(dialogs(*portal, 0), 0);
}

/** :53-56: with its key, the dialog closes (and the player is teleported) */
TEST_F(PadmarashkaDanuarAiTest, TheStartPortalWithItsKey) {
	open(SEIZED_DANUAR_SANCTUARY);
	for (const auto& [portalId, keyId] : std::vector<std::pair<int32_t, int32_t>>{{701871, 185000183}, {701872, 185000182}, {701873, 185000181}}) {
		runtime::Ptr<Npc> portal = place(portalId, 3.0f);
		DialogCaveProbe<dan::StartTeleportAI>& ai = install<DialogCaveProbe<dan::StartTeleportAI>>(*portal);
		services::item::ItemService::addItem(player(), keyId, 1);
		(*client)->clearSent();
		EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
		EXPECT_EQ(dialogs(*portal, 0), 1) << portalId;
		EXPECT_EQ(dialogs(*portal, model::id(model::DialogPage::NO_RIGHT)), 0) << portalId;
	}
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
