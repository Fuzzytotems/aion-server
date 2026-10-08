// M5j stage 2 CP3, group K (m5j-plan.md §18.3, item E-09): the express mail postman's AIs FollowingNpcAI ("following") and DeliveryManAI
// ("deliveryman") (P5-05). A postman npc of the deliveryman template row (npc_templates.xml's 798100) created by
// the item fixture's player, who is registered in the postman's map instance and stored in the World.
//
// Java: data/handlers/ai/FollowingNpcAI.java, DeliveryManAI.java.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/ai/DeliveryManAI.h"
#include "aion/gameserver/handlers/ai/FollowingNpcAI.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace roots = gameserver::handlers::ai;

using gameserver::ai::AIState;
using gameserver::ai::event::AIEventType;
using model::gameobjects::Npc;

constexpr int32_t POSTMAN = 798100;

/** npc_templates.xml:462252-462258, the Elyos postman (798100) */
constexpr const char* POSTMAN_XML =
	R"(<npc_templates><npc_template npc_id="798100" level="15" name="zephyr deliveryman" name_id="350579" height="1.16875" group_drop="NONE" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="deliveryman" srange="20" sangle="240" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="2256"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" )"
	R"(group_run_fight="4.23" /></stats><bound_radius front="0.595" side="0.3774" upper="1.16875" /><talk_info distance="5" )"
	R"(can_talk_invisible="false" /></npc_template></npc_templates>)";

class PostmanSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	PostmanSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** DeliveryManAI with its protected hooks callable */
class DeliveryManAiProbe final : public roots::DeliveryManAI {
public:
	using DeliveryManAI::DeliveryManAI;
	using DeliveryManAI::canHandleEvent;
	using DeliveryManAI::handleDespawned;
	using DeliveryManAI::handleDialogStart;
	using DeliveryManAI::handleSpawned;
};

class PostmanAiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // the empty AI registry of this executable: the test installs the AI
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, POSTMAN_XML));
		player().setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
		player().setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(player()));
		world::World::getInstance().storeObject(player());
		clearSent();
	}

	void TearDown() override {
		for (const runtime::Ref<Npc>& npc : npcs)
			npc->getController().cancelAllTasks();
		player().setPostman(nullptr);
		mapInstance->removeObject(player());
		world::World::getInstance().removeObject(player());
		npcs.clear();
		groups.clear();
		dataholders::DataManager::NPC_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	/** the postman of `creatorId` beside the player, its DeliveryManAI installed and idle */
	Npc& postman(int32_t creatorId) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, POSTMAN, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<PostmanSpawnTemplate>(*group, 102.0f, 100.0f, 50.0f));
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(POSTMAN));
		npc->setCreatorId(creatorId);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, 102.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(102.0f, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		auto ai = std::make_unique<DeliveryManAiProbe>(*npc);
		ai->setStateIfNot(AIState::IDLE);
		npc->replaceAi(std::move(ai));
		groups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	DeliveryManAiProbe& aiOf(Npc& npc) { return dynamic_cast<DeliveryManAiProbe&>(npc.getAi()); }

	xml::LoadContext context;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groups;
	std::vector<runtime::Ref<Npc>> npcs;
};

/** DeliveryManAI.java:51-58: its player gets the mailbox in express mode; anybody else does not */
TEST_F(PostmanAiTest, TheDialogOpensTheExpressMailboxForItsPlayerOnly) {
	mapInstance->addObject(player());
	Npc& stranger = postman(player().getObjectId() + 1);
	aiOf(stranger).handleDialogStart(player());
	EXPECT_NE(player().getMailbox()->mailBoxState.get(), services::player::PlayerMailboxState::EXPRESS);
	EXPECT_TRUE(sent().empty()) << "the 'no mail for you' line goes to the players who see the postman";

	Npc& mine = postman(player().getObjectId());
	aiOf(mine).handleDialogStart(player());
	EXPECT_EQ(player().getMailbox()->mailBoxState.get(), services::player::PlayerMailboxState::EXPRESS);
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_DIALOG_WINDOW(mine.getObjectId(), model::id(model::DialogPage::MAIL)))}));
}

/** DeliveryManAI.java:29-40, FollowingNpcAI.java:24-48: the postman follows its player and despawns after its service time */
TEST_F(PostmanAiTest, ASpawnedPostmanFollowsItsPlayer) {
	mapInstance->addObject(player());
	Npc& npc = postman(player().getObjectId());
	aiOf(npc).handleSpawned();
	EXPECT_EQ(aiOf(npc).getState(), AIState::FOLLOWING);
	ASSERT_TRUE(npc.getTarget());
	EXPECT_TRUE(npc.getTarget()->equals(player()));
	EXPECT_TRUE(npc.getController().hasTask(model::TaskId::DESPAWN));
	EXPECT_TRUE(aiOf(npc).canHandleEvent(AIEventType::CREATURE_MOVED)) << "FOLLOWING";
	EXPECT_TRUE(aiOf(npc).canHandleEvent(AIEventType::DIALOG_START));
}

/** FollowingNpcAI.java:29-39: outside FOLLOWING the moves are not handled */
TEST_F(PostmanAiTest, AnIdlePostmanIgnoresMoves) {
	Npc& npc = postman(player().getObjectId());
	EXPECT_FALSE(aiOf(npc).canHandleEvent(AIEventType::CREATURE_MOVED));
}

/** DeliveryManAI.java:33-36 with a player who is not in the postman's instance: Java's NullPointerException after the follow state */
TEST_F(PostmanAiTest, APostmanWithoutItsPlayerThrows) {
	Npc& npc = postman(player().getObjectId());
	EXPECT_THROW(aiOf(npc).handleSpawned(), runtime::NullPointerException);
	EXPECT_EQ(aiOf(npc).getState(), AIState::FOLLOWING) << "FollowEventHandler.follow ran with the null creature";
}

/** DeliveryManAI.java:42-49: the despawn releases the player's postman, only if it is this one */
TEST_F(PostmanAiTest, TheDespawnReleasesThePlayersPostman) {
	Npc& other = postman(player().getObjectId());
	Npc& mine = postman(player().getObjectId());
	player().setPostman(runtime::Ptr<Npc>(mine));
	aiOf(other).handleDespawned();
	EXPECT_TRUE(player().getPostman()) << "another postman's despawn";
	aiOf(mine).handleDespawned();
	EXPECT_FALSE(player().getPostman());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
