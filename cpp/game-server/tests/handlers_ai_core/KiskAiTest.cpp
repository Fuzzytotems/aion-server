// M5j stage 2 CP2 (m5j-plan.md §18.3, item E-05): kisks - KiskService.removeKisk and onBind (P5-08) and the AIs KiskAI ("kisk") and
// InvisiblekiskAI ("invisible_kisk") (P5-05). A Kisk object of a kisk template row (npc_templates.xml:440100-440107, the portable kisk with
// its members count changed per case) placed beside the item fixture's player, who is stored in the World so that the kisk finds its members.
//
// Java: services/KiskService.java:28-63, data/handlers/ai/KiskAI.java, InvisiblekiskAI.java.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/ai/InvisiblekiskAI.h"
#include "aion/gameserver/handlers/ai/KiskAI.h"
#include "aion/gameserver/model/animations/ActionAnimation.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_KISK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/KiskService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace roots = gameserver::handlers::ai;

using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Kisk;
using network::aion::serverpackets::SM_ACTION_ANIMATION;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t KISK = 700118;      // two members
constexpr int32_t FULL_KISK = 700119; // no member at all
constexpr int32_t SM_KISK_UPDATE_OPCODE = ::aion::gameserver::network::aion::opcodeOf<serverpackets::SM_KISK_UPDATE>;
constexpr int32_t SM_QUESTION_WINDOW_OPCODE = ::aion::gameserver::network::aion::opcodeOf<SM_QUESTION_WINDOW>;

std::string kiskRow(int32_t npcId, int32_t members) {
	// npc_templates.xml:440100-440107 with the members count of the case and usemask 0 (Kisk.isUseAllowed: no restriction)
	return "<npc_template npc_id=\"" + std::to_string(npcId) +
	       "\" level=\"1\" name=\"portable kisk\" name_id=\"350916\" height=\"2\" group_drop=\"NONE\" rank=\"DISCIPLINED\" rating=\"NORMAL\" "
	       "race=\"ELYOS\" tribe=\"GENERAL\" type=\"GENERAL\" ai=\"kisk\" srange=\"15\" attack_speed=\"2000\" hpgauge=\"3\"><stats maxHp=\"2961\">"
	       "<speeds walk=\"1.5\" group_walk=\"1.5\" run=\"6\" run_fight=\"4.2\" group_run_fight=\"4.2\" /></stats><kisk_stats resurrects=\"0\" "
	       "members=\"" + std::to_string(members) + "\" usemask=\"0\" /><bound_radius front=\"0.25\" side=\"0.35\" upper=\"2\" />"
	       "<talk_info distance=\"5\" can_talk_invisible=\"false\" /></npc_template>";
}

class KiskSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	KiskSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** KiskAI with its protected hooks callable */
class KiskAiProbe final : public roots::KiskAI {
public:
	using KiskAI::KiskAI;
	using KiskAI::handleAttack;
	using KiskAI::handleDespawned;
	using KiskAI::handleDialogStart;
};

class KiskAiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		// this executable links the empty AI registry: the kisk's npc gets a DummyAI and the test installs the KiskAI (TravelAiHandlersTest's way)
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		dataholders::DataManager::NPC_DATA.publish(
			xml::bindString<dataholders::NpcData>(context, "<npc_templates>" + kiskRow(KISK, 2) + kiskRow(FULL_KISK, 0) + "</npc_templates>"));
		player().setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
		world::World::getInstance().storeObject(player());
		clearSent();
	}

	void TearDown() override {
		player().setKisk(nullptr);
		world::World::getInstance().removeObject(player());
		kisks.clear();
		groups.clear();
		dataholders::DataManager::NPC_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	/** a Kisk of the template placed by the player, the player its creator; its KiskAI installed and idle */
	Kisk& kiskOf(int32_t npcId) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<KiskSpawnTemplate>(*group, 101.0f, 100.0f, 50.0f));
		runtime::Ref<Kisk> kisk = model::gameobjects::VisibleObject::create<Kisk>(std::make_unique<controllers::NpcController>(), spawn, player());
		kisk->setPosition(world::WorldPosition::create(210010000, 101.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(101.0f, 100.0f, 50.0f)));
		kisk->getPosition()->setIsSpawned(true);
		auto ai = std::make_unique<KiskAiProbe>(*kisk);
		ai->setStateIfNot(gameserver::ai::AIState::IDLE);
		kisk->replaceAi(std::move(ai));
		groups.push_back(group);
		kisks.push_back(kisk);
		return *kisk;
	}

	KiskAiProbe& aiOf(Kisk& kisk) { return dynamic_cast<KiskAiProbe&>(kisk.getAi()); }

	int32_t count(int32_t opcode) {
		int32_t n = 0;
		for (const SerializedBody& body : (*client)->sent())
			n += body.opCode == opcode ? 1 : 0;
		return n;
	}

	bool wasSent(const std::vector<uint8_t>& packet) {
		for (const std::vector<uint8_t>& bytes : sent())
			if (bytes == packet)
				return true;
		return false;
	}

	xml::LoadContext context;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groups;
	std::vector<runtime::Ref<Kisk>> kisks;
};

/** KiskAI.java:49-77, KiskService.java:54-63: the question, a yes binds (the message, the bind flash, the bind point), a second click knows */
TEST_F(KiskAiTest, AClickAsksAndAYesBinds) {
	Kisk& kisk = kiskOf(KISK);
	aiOf(kisk).handleDialogStart(player());
	EXPECT_EQ(count(SM_QUESTION_WINDOW_OPCODE), 1) << "STR_ASK_REGISTER_BINDSTONE";
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_ASK_REGISTER_BINDSTONE, 1));
	ASSERT_TRUE(player().getKisk());
	EXPECT_TRUE(player().getKisk()->equals(kisk));
	EXPECT_EQ(kisk.getCurrentMemberCount(), 1);
	EXPECT_TRUE(wasSent(serializedFor(SM_SYSTEM_MESSAGE::STR_BINDSTONE_REGISTER())));
	EXPECT_TRUE(wasSent(serializedFor(SM_ACTION_ANIMATION(player().getObjectId(), model::animations::ActionAnimation::BIND_KISK))));
	EXPECT_GE(count(SM_KISK_UPDATE_OPCODE), 1) << "Kisk.addPlayer's update";

	clearSent();
	aiOf(kisk).handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_BINDSTONE_ALREADY_REGISTERED())}));
}

/** KiskAI.java:73-76: a no takes the decision; the player stays unbound */
TEST_F(KiskAiTest, ANoLeavesThePlayerUnbound) {
	Kisk& kisk = kiskOf(KISK);
	aiOf(kisk).handleDialogStart(player());
	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_ASK_REGISTER_BINDSTONE, 0));
	EXPECT_FALSE(player().getKisk());
	EXPECT_EQ(kisk.getCurrentMemberCount(), 0);
}

/**
 * KiskAI.java:73-76 with AIActions.java:99-107: walking out of the 5 m range denies the request (DialogObserver.tooFar) without removing the
 * question, so a later yes finds the decision taken and binds nothing
 */
TEST_F(KiskAiTest, AYesAfterWalkingAwayBindsNothing) {
	Kisk& kisk = kiskOf(KISK);
	aiOf(kisk).handleDialogStart(player());
	player().setPosition(world::WorldPosition::create(210010000, 120.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(120.0f, 100.0f, 50.0f)));
	player().getPosition()->setIsSpawned(true);
	player().getObserveController()->notifyMoveObservers();
	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_ASK_REGISTER_BINDSTONE, 1)) << "the question is still open";
	EXPECT_FALSE(player().getKisk());
	EXPECT_EQ(kisk.getCurrentMemberCount(), 0);
}

/** KiskAI.java:79-82: a kisk without room answers FULL; KiskService.onBind leaves the previous kisk */
TEST_F(KiskAiTest, AFullKiskRefusesAndABindMovesFromThePreviousKisk) {
	Kisk& full = kiskOf(FULL_KISK);
	aiOf(full).handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_BINDSTONE_FULL())}));

	Kisk& first = kiskOf(KISK);
	Kisk& second = kiskOf(KISK);
	services::KiskService::getInstance().onBind(first, player());
	services::KiskService::getInstance().onBind(second, player());
	EXPECT_EQ(first.getCurrentMemberCount(), 0) << "removePlayer of the previous kisk";
	EXPECT_EQ(second.getCurrentMemberCount(), 1);
	EXPECT_TRUE(player().getKisk()->equals(second));
}

/** KiskAI.java:33-37: an attack on a kisk with full HP warns its members */
TEST_F(KiskAiTest, AnAttackAtFullHpWarnsTheMembers) {
	Kisk& kisk = kiskOf(KISK);
	services::KiskService::getInstance().onBind(kisk, player());
	clearSent();
	aiOf(kisk).handleAttack(nullptr);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_BINDSTONE_IS_ATTACKED())}));
}

/**
 * KiskAI.java:39-47, KiskService.java:28-52: a despawn removes the kisk (the owner registration, the creator's update, the members' bind
 * point and kisk) and tells the members it was removed
 */
TEST_F(KiskAiTest, ADespawnRemovesTheKisk) {
	Kisk& kisk = kiskOf(KISK);
	services::KiskService& service = services::KiskService::getInstance();
	service.regKisk(kisk, player().getObjectId());
	service.onBind(kisk, player());
	ASSERT_TRUE(service.haveKisk(player().getObjectId()));
	clearSent();
	aiOf(kisk).handleDespawned();
	EXPECT_FALSE(service.haveKisk(player().getObjectId())) << "ownerPlayer";
	EXPECT_FALSE(player().getKisk());
	EXPECT_GE(count(SM_KISK_UPDATE_OPCODE), 1) << "the creator's SM_KISK_UPDATE";
	EXPECT_TRUE(wasSent(serializedFor(SM_SYSTEM_MESSAGE::STR_BINDSTONE_IS_REMOVED())));
	EXPECT_FALSE(wasSent(serializedFor(SM_SYSTEM_MESSAGE::STR_BINDSTONE_IS_DESTROYED())));
}

/** KiskAI.java:85-92 */
TEST_F(KiskAiTest, TheQuestions) {
	Kisk& kisk = kiskOf(KISK);
	KiskAiProbe& ai = aiOf(kisk);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE));
	EXPECT_TRUE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
