// I6-1 (docs/design/p6-i6-plan.md §3): the twelve AIs of ai/instance/rakes (game-server/data/handlers/ai/instance/rakes) on npcs of a real Steel
// Rake instance, each AI installed with replaceAi. Left to a gate: the walk of Tamer Anikiki (no walker rows here), the move to a supply device
// (the move controller), the casts' effects, the use bars (DialogService) and the fly teleport itself.

#include "I6TestSupport.h"

#include <any>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/BigBadaboomAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/BrassEyeGroggetAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/CentralDeckMobileCannonAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/ChiefGunnerKoakoaAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/EngineerLahulahuAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/FeedingMantutuAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/GeniesIncenseBurnerAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/GoldenEyeMantutuAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/SteelRakeKeyBoxAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/SuspiciousCannonAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/TamerAnikikiAI.h"
#include "aion/gameserver/handlers/ai/instance/rakes/WalkingTalkingBombAI.h"
#include "aion/gameserver/handlers/instance/rakes/SteelRakeInstance.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/ai/Percentage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemFactory.h"

#define AION_RAKES_AIS(X)                                                                                                                     \
	X(BigBadaboomAI)                                                                                                                          \
	X(BrassEyeGroggetAI)                                                                                                                      \
	X(CentralDeckMobileCannonAI)                                                                                                              \
	X(ChiefGunnerKoakoaAI)                                                                                                                    \
	X(EngineerLahulahuAI)                                                                                                                     \
	X(FeedingMantutuAI)                                                                                                                       \
	X(GeniesIncenseBurnerAI)                                                                                                                  \
	X(GoldenEyeMantutuAI)                                                                                                                     \
	X(SteelRakeKeyBoxAI)                                                                                                                      \
	X(SuspiciousCannonAI)                                                                                                                     \
	X(TamerAnikikiAI)                                                                                                                         \
	X(WalkingTalkingBombAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::instance::rakes {                                                                               \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_RAKES_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace rk = ::aion::gameserver::handlers::ai::instance::rakes;
using gameserver::ai::AIState;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// the protected handlers the cases drive directly (Java: the AI's events)
template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleBackHome;
	using AI::handleDied;
	using AI::handleSpawned;
};
template <class AI>
struct UseProbe : AI {
	using AI::AI;
	using AI::handleSpawned;
	using AI::handleUseItemFinish;
};
template <class AI>
struct SummonerProbe : AI {
	using AI::AI;
	using AI::handleIndividualSpawnedSummons;
};
struct MantutuProbe : rk::GoldenEyeMantutuAI {
	using GoldenEyeMantutuAI::GoldenEyeMantutuAI;
	using GoldenEyeMantutuAI::handleAttack;
	using GoldenEyeMantutuAI::handleBackHome;
	using GoldenEyeMantutuAI::handleDied;
	using GoldenEyeMantutuAI::handleMoveArrived;
};
struct AnikikiProbe : rk::TamerAnikikiAI {
	using TamerAnikikiAI::TamerAnikikiAI;
	using TamerAnikikiAI::handleCreatureMoved;
	using TamerAnikikiAI::handleSpawned;
};

class RakesAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(STEEL_RAKE, rakes::SteelRakeInstance_instanceFactory);
		enter(*instance, 700.0f, 500.0f, 940.0f);
	}

	void TearDown() override {
		InstanceHandlerTest::TearDown();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 705.0f, float y = 500.0f, float z = 940.0f) {
		return spawnNpc(*instance, npcId, x, y, z, int8_t{0});
	}

	size_t count(int32_t id) { return npcs(*instance, id).size(); }

	/** SKILL_DATA with one plain template per id (the base fixture publishes an empty holder; its TearDown resets this one) */
	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="rakes" nameId="1" stack="RAKES" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	/** Largimark's Flint (item_templates.xml:895928) in the player's cube */
	void giveFlint() {
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext,
			R"(<item_templates><item_template id="185000052" name="Largimark's Flint" level="1" cName="key_idshulack_f3_d10" mask="20553" item_group="KEY" quality="COMMON" price="1" desc="746722"/></item_templates>)"));
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		ASSERT_TRUE(player().getInventory().add(*services::item::ItemFactory::newItem(185000052, 1), runtime::Ptr<model::gameobjects::player::Player>(player())));
	}

	void aggro(Npc& npc) {
		InstanceKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
	}

	xml::LoadContext skillContext;
	xml::LoadContext itemContext;
	world::WorldMapInstance* instance = nullptr;
};

/** The twelve AION_AI factories build their classes */
TEST_F(RakesAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = rk::cls##_aiFactory(*npc);                                                                                                \
		EXPECT_NE(dynamic_cast<rk::cls*>(ai.get()), nullptr) << #cls;                                                                         \
	}
	AION_RAKES_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** WalkingTalkingBombAI.java:19-48: alive 2 s after its spawn it casts on itself, 4 s later it is gone; no decay, respawn or rewards */
TEST_F(RakesAiTest, TheWalkingTalkingBombGoesOffAfterSixSeconds) {
	publishSkills({19416});
	runtime::Ptr<Npc> bomb = place(900200);
	auto& ai = install<Probe<rk::WalkingTalkingBombAI>>(*bomb);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(5999));
	EXPECT_TRUE(bomb->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(bomb->isSpawned());
}

/** SteelRakeKeyBoxAI.java:20-33: a key box left alone is deleted 3 minutes after its spawn */
TEST_F(RakesAiTest, AKeyBoxLastsThreeMinutes) {
	runtime::Ptr<Npc> box = place(700553);
	auto& ai = install<UseProbe<rk::SteelRakeKeyBoxAI>>(*box);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(179999));
	EXPECT_TRUE(box->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(box->isSpawned());
}

/** CentralDeckMobileCannonAI.java:22-39: without the flint message 1111302; with it on the Steel Rake the crew dies and Menekiki comes */
TEST_F(RakesAiTest, TheCentralCannonClearsTheDeck) {
	runtime::Ptr<Npc> cannon = place(900200);
	auto& ai = install<UseProbe<rk::CentralDeckMobileCannonAI>>(*cannon);
	runtime::Ptr<Npc> gunner = place(215402, 710.0f);
	runtime::Ptr<Npc> other = place(900300, 712.0f);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1111302, std::vector<std::string>{}))), 1);
	EXPECT_EQ(count(215069), 0u);
	giveFlint();
	ai.handleUseItemFinish(player());
	EXPECT_EQ(player().getInventory().getItemCountByItemId(185000052), 0);
	EXPECT_TRUE(gunner->isDead());
	EXPECT_FALSE(other->isDead());
	ASSERT_EQ(count(215069), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 215069)[0]->getZ(), 1032.84f);
}

/** SuspiciousCannonAI.java:23-30: the user flies off on teleport 73001 (247001 from the cannon 730769) */
TEST_F(RakesAiTest, ASuspiciousCannonShootsTheUserOff) {
	runtime::Ptr<Npc> cannon = place(730769);
	auto& ai = install<UseProbe<rk::SuspiciousCannonAI>>(*cannon);
	ai.handleUseItemFinish(player());
	EXPECT_TRUE(player().isInState(CreatureState::FLYING));
	EXPECT_FALSE(player().isInState(CreatureState::ACTIVE));
	EXPECT_EQ(countSent(serialized(SM_EMOTION(player(), EmotionType::START_FLYTELEPORT, 247001, 0))), 1);
	runtime::Ptr<Npc> other = place(900200);
	auto& otherAi = install<UseProbe<rk::SuspiciousCannonAI>>(*other);
	otherAi.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_EMOTION(player(), EmotionType::START_FLYTELEPORT, 73001, 0))), 1);
}

/** BigBadaboomAI.java:18-29, GeniesIncenseBurnerAI.java:20-24: the badaboom's morph and deletion, the burner's cast at itself */
TEST_F(RakesAiTest, TheBadaboomAndTheBurner) {
	publishSkills({20048, 18465});
	runtime::Ptr<Npc> badaboom = place(231016);
	auto& ai = install<UseProbe<rk::BigBadaboomAI>>(*badaboom);
	ai.handleUseItemFinish(player());
	EXPECT_FALSE(badaboom->isSpawned());
	runtime::Ptr<Npc> burner = place(900200);
	auto& burnerAi = install<UseProbe<rk::GeniesIncenseBurnerAI>>(*burner);
	burnerAi.handleUseItemFinish(player());
	EXPECT_EQ(burner->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(burner));
}

/** FeedingMantutuAI.java:29-46, GoldenEyeMantutuAI.java:43-56: a supply used while Mantutu lives spawns its device and sends him to it */
TEST_F(RakesAiTest, ASupplySendsMantutuToItsDevice) {
	runtime::Ptr<Npc> mantutu = place(219033, 715.0f, 500.0f);
	auto& mantutuAi = install<MantutuProbe>(*mantutu);
	runtime::Ptr<Npc> water = place(701387);
	auto& ai = install<UseProbe<rk::FeedingMantutuAI>>(*water);
	ai.handleUseItemFinish(player());
	ASSERT_EQ(count(281129), 1u);
	EXPECT_FALSE(water->isSpawned());
	EXPECT_FALSE(mantutuAi.canThink());
	EXPECT_EQ(mantutu->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(npcs(*instance, 281129)[0]));
	EXPECT_EQ(mantutuAi.getState(), AIState::FOLLOWING);
	mantutu->getController().delete_();
	runtime::Ptr<Npc> feed = place(701386);
	auto& feedAi = install<UseProbe<rk::FeedingMantutuAI>>(*feed);
	feedAi.handleUseItemFinish(player());
	EXPECT_EQ(count(281128), 0u) << "without Mantutu nothing happens";
	EXPECT_TRUE(feed->isSpawned());
}

/** GoldenEyeMantutuAI.java:58-101: arrived at the feed device he eats 6 s, the device goes, the feed supply comes back and he thinks again */
TEST_F(RakesAiTest, MantutuEatsAtTheDevice) {
	publishSkills({20489, 20490});
	runtime::Ptr<Npc> mantutu = place(219033, 715.0f, 500.0f);
	auto& ai = install<MantutuProbe>(*mantutu);
	EXPECT_TRUE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	runtime::Ptr<Npc> device = place(281128, 714.62634f, 504.4552f, 939.60675f);
	mantutu->getAi().onCustomEvent(1, {std::any(runtime::Ref<Npc>(*device))}); // FeedingMantutuAI's call
	ai.handleMoveArrived();
	executor().advance(std::chrono::milliseconds(5999));
	EXPECT_TRUE(device->isSpawned());
	EXPECT_FALSE(ai.canThink());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(device->isSpawned());
	EXPECT_EQ(count(701386), 1u);
	EXPECT_TRUE(ai.canThink());
}

/** GoldenEyeMantutuAI.java:111-156: his first attack starts the hunger round; back home stops it and he thinks again */
TEST_F(RakesAiTest, MantutusHungerRound) {
	publishSkills({20489, 20490, 18189});
	runtime::Ptr<Npc> mantutu = place(219033, 715.0f, 500.0f);
	auto& ai = install<MantutuProbe>(*mantutu);
	aggro(*mantutu);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	executor().advance(std::chrono::seconds(40)); // two casts at 10 s and 40 s
	mantutu->getAi().onCustomEvent(1, {std::any(runtime::Ref<Npc>(*place(281128)))}); // sent to a device: he stops thinking
	ASSERT_FALSE(ai.canThink());
	ai.handleBackHome();
	EXPECT_TRUE(ai.canThink()) << "back home he thinks again";
	runtime::Ptr<Npc> keeper = place(219037, 720.0f);
	ai.handleDied(); // takes 18189 off 219037 (none here: a no-op)
	EXPECT_TRUE(keeper->isSpawned());
}

/** BrassEyeGroggetAI.java:27-111: at 70 % the first wave 9 s later (281184, 281181), the second 35 s after it (281183, 281184, 281181) */
TEST_F(RakesAiTest, BrassEyeGroggetsWaves) {
	runtime::Ptr<Npc> grogget = place(900200);
	auto& ai = install<SummonerProbe<rk::BrassEyeGroggetAI>>(*grogget);
	model::templates::ai::Percentage percent;
	percent.percent = 70;
	ai.handleIndividualSpawnedSummons(percent);
	executor().advance(std::chrono::milliseconds(8999));
	EXPECT_EQ(count(281184), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(281184), 1u);
	EXPECT_EQ(count(281181), 1u);
	executor().advance(std::chrono::seconds(35));
	EXPECT_EQ(count(281183), 1u);
	EXPECT_EQ(count(281184), 2u);
	EXPECT_EQ(count(281181), 2u);
	percent.percent = 20;
	ai.handleIndividualSpawnedSummons(percent);
	executor().advance(std::chrono::seconds(9));
	EXPECT_EQ(count(281182), 2u) << "the third wave";
	executor().advance(std::chrono::seconds(35));
	EXPECT_EQ(count(281187), 1u);
}

/** ChiefGunnerKoakoaAI.java:21-93: each summons percentage fills the fifteen places with 281212 and 281213 */
TEST_F(RakesAiTest, ChiefGunnerKoakoaCallsHisGunners) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	runtime::Ptr<Npc> koakoa = place(900200);
	auto& ai = install<SummonerProbe<rk::ChiefGunnerKoakoaAI>>(*koakoa);
	model::templates::ai::Percentage percent;
	percent.percent = 50;
	for (int32_t round = 1; round <= 3; round++) {
		ai.handleIndividualSpawnedSummons(percent);
		EXPECT_EQ(count(281212) + count(281213), static_cast<size_t>(15 * round));
	}
	EXPECT_GE(count(281212), 3u) << "the fixed one each round";
}

/** EngineerLahulahuAI.java:39-157: at 95 % he finds his devices and a round begins; every 10 s one or two of them target themselves */
TEST_F(RakesAiTest, EngineerLahulahusDevices) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	publishSkills({18131, 18132, 18153});
	std::vector<runtime::Ptr<Npc>> devices;
	for (int32_t id : {281108, 281109, 281110, 281111, 281112, 281113, 281114, 281322, 281323, 281324, 281325, 281326})
		devices.push_back(place(id, 710.0f + static_cast<float>(devices.size())));
	for (const runtime::Ptr<Npc>& device : devices)
		install<gameserver::handlers::ai::GeneralNpcAI>(*device); // their casts need an NpcAI (a DummyAI cannot use a skill)
	runtime::Ptr<Npc> lahulahu = place(900200);
	auto& ai = install<Probe<rk::EngineerLahulahuAI>>(*lahulahu);
	aggro(*lahulahu);
	auto targeted = [&devices] {
		int32_t n = 0;
		for (const runtime::Ptr<Npc>& device : devices)
			n += device->getTarget() == runtime::Ptr<model::gameobjects::VisibleObject>(device) ? 1 : 0;
		return n;
	};
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(targeted(), 0) << "at full HP no phase";
	lahulahu->getLifeStats()->setCurrentHpPercent(90);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	int32_t first = targeted();
	EXPECT_GE(first, 1);
	EXPECT_LE(first, 2);
	for (int32_t round = 0; round < 10; round++)
		executor().advance(std::chrono::seconds(10));
	EXPECT_GT(targeted(), first) << "the rounds go on every 10 s";
	ai.handleDied();
}

/** TamerAnikikiAI.java:34-99: no thinking; only 219037 takes full damage; the others cast 18189 at themselves 5 s after their spawn */
TEST_F(RakesAiTest, TheTamersBeasts) {
	publishSkills({18189});
	runtime::Ptr<Npc> beast = place(219037);
	auto& ai = install<AnikikiProbe>(*beast);
	EXPECT_FALSE(ai.canThink());
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 500.0f, nullptr), 500.0f);
	ai.handleSpawned();
	executor().advance(std::chrono::seconds(5));
	runtime::Ptr<Npc> other = place(900200);
	auto& otherAi = install<AnikikiProbe>(*other);
	EXPECT_FLOAT_EQ(otherAi.modifyDamage(player(), 500.0f, nullptr), 1.0f);
}

/** TamerAnikikiAI.java:39-57: a player moving within 10 m of Anikiki (219040) starts his event once: four key boxes, the quest timer, a message */
TEST_F(RakesAiTest, AnikikiStartsHisRunOnce) {
	runtime::Ptr<Npc> anikiki = place(219040, 708.0f);
	auto& ai = install<AnikikiProbe>(*anikiki);
	runtime::Ptr<Npc> far = place(219040, 760.0f);
	auto& farAi = install<AnikikiProbe>(*far);
	farAi.handleCreatureMoved(player());
	EXPECT_EQ(count(700553), 0u) << "60 m away";
	ai.handleCreatureMoved(*place(900200, 709.0f)); // an npc does not start it
	EXPECT_EQ(count(700553), 0u);
	ai.handleCreatureMoved(player());
	EXPECT_EQ(count(700553), 4u);
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 180))), 1);
	ai.handleCreatureMoved(player());
	EXPECT_EQ(count(700553), 4u) << "once";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
