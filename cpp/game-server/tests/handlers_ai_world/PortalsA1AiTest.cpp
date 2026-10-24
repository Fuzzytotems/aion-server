// A1 portals (lane A, 2026-10-16): the portal AIs of chunk A1 (game-server/data/handlers/ai/portals) on npcs of a Poeta map instance, each
// installed through its AION_AI factory. The user is the player of tests/cm_ak/ItemPacketTestSupport.h, moved into that instance: a real
// AionConnection whose send queue the cases read. The npc rows are synthetic (npc_templates.xml's shape, the attributes the AIs read).

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/ai/portals/DramataPortalAI.h"
#include "aion/gameserver/handlers/ai/portals/GroupGateAI.h"
#include "aion/gameserver/handlers/ai/portals/LegionDominionPortalAI.h"
#include "aion/gameserver/handlers/ai/portals/OccupiedRentusBasePortalAI.h"
#include "aion/gameserver/handlers/ai/portals/PortalElevatorAI.h"
#include "aion/gameserver/handlers/ai/portals/PortalRequestAI.h"
#include "aion/gameserver/handlers/ai/portals/SealedDanuarMysticariumPortals.h"
#include "aion/gameserver/handlers/ai/portals/TAGPortalAI.h"
#include "aion/gameserver/handlers/ai/portals/abyssalsplinter/AbyssalSplinterPortalAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them
namespace aion::gameserver::handlers::ai::portals {
::aion::gameserver::handlers::AIFactory GroupGateAI_aiFactory;
::aion::gameserver::handlers::AIFactory DramataPortalAI_aiFactory;
::aion::gameserver::handlers::AIFactory LegionDominionPortalAI_aiFactory;
::aion::gameserver::handlers::AIFactory OccupiedRentusBasePortalAI_aiFactory;
::aion::gameserver::handlers::AIFactory PortalElevatorAI_aiFactory;
::aion::gameserver::handlers::AIFactory PortalRequestAI_aiFactory;
::aion::gameserver::handlers::AIFactory SealedDanuarMysticariumPortals_aiFactory;
::aion::gameserver::handlers::AIFactory TAGPortalAI_aiFactory;
namespace abyssalsplinter {
::aion::gameserver::handlers::AIFactory AbyssalSplinterPortalAI_aiFactory;
} // namespace abyssalsplinter
} // namespace aion::gameserver::handlers::ai::portals

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace portals = gameserver::handlers::ai::portals;

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t ASK_GROUP_GATE_MOVE = 160014; // SM_QUESTION_WINDOW.STR_ASK_GROUP_GATE_DO_YOU_ACCEPT_MOVE

/** One synthetic row per portal npc the cases spawn (a portal's shape: an Elyos field object with a talk distance) */
std::string portalNpcTemplatesXml() {
	std::string xml = "<npc_templates>";
	for (int32_t id : {833207, 833208, 749017, 749083, 749131, 749132, 832991, 832992, 900100, 730721, 731583})
		xml += R"(<npc_template npc_id=")" + std::to_string(id) +
			R"(" level="1" name="portal" name_id="1" height="1" rank="SEASONED" rating="NORMAL" race="NONE" tribe="FIELD_OBJECT_ALL" type="GENERAL">)"
			R"(<stats maxHp="100"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2"/></stats>)"
			R"(<bound_radius front="0.5" side="0.5" upper="1"/><talk_info distance="10" is_dialog="true"/></npc_template>)";
	return xml + "</npc_templates>";
}

const model::templates::npc::NpcTemplate* templateOf(int32_t npcId) {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, portalNpcTemplatesXml()).release();
	}();
	return holder->getNpcTemplate(npcId);
}

class PortalSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	PortalSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class PortalsA1AiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		// the world holders are published once per process (tests/ai/AiWorldTestSupport.h), first: the suites' order stays irrelevant
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		// Player::postConstruct loads the toy pets from the database; the tests have none
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		useInstance = world::WorldMap2DInstance::create(*map, 2, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
		player().setPosition(
			world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
		clearSent();
	}

	void TearDown() override {
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		if (playerInWorld)
			world::World::getInstance().removeObject(player());
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		useInstance->detachInstanceHandler();
		useInstance = nullptr;
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	/** An npc of the row, spawned 2 m east of the player (Java VisibleObjectSpawner.spawnNpc) */
	Npc& npcAt(int32_t npcId, float x = 102.0f) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<PortalSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, templateOf(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	/** The player in World: Npc.getCreator finds a gate's creator there */
	void playerIntoWorld() {
		world::World::getInstance().storeObject(player());
		playerInWorld = true;
	}

	std::vector<uint8_t> serializedFor(AionServerPacket&& packet) { return serialized(std::move(packet), client->con()); }

	/** The call throws, and its message names `text` (a teleport stopped by the test world, which has Poeta's map only) */
	template <class F>
	static void expectThrowNaming(F&& call, const std::string& text) {
		try {
			call();
			ADD_FAILURE() << "no throw: expected one naming " << text;
		} catch (const std::exception& e) {
			EXPECT_NE(std::string(e.what()).find(text), std::string::npos) << e.what();
		}
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	runtime::Ref<world::WorldMapInstance> useInstance;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
	bool playerInWorld = false;
};

// ---- GroupGateAI (GroupGateAI.java:30-75) ---------------------------------------------------------------------------------------------------

/** GroupGateAI with its protected hook callable */
class GroupGateProbe final : public portals::GroupGateAI {
public:
	using GroupGateAI::GroupGateAI;
	using GroupGateAI::handleDialogStart;
};

TEST_F(PortalsA1AiTest, TheGroupGateMarkerBuildsAGroupGate) {
	Npc& gate = npcAt(833207);
	std::unique_ptr<gameserver::ai::AbstractAI> ai = portals::GroupGateAI_aiFactory(gate);
	EXPECT_NE(dynamic_cast<portals::GroupGateAI*>(ai.get()), nullptr) << "the factory of the \"groupgate\" marker";
}

/** :32-35: a player who is no Daeva cannot use it */
TEST_F(PortalsA1AiTest, AGroupGateIsForDaevasOnly) {
	playerIntoWorld();
	Npc& gate = npcAt(833207);
	gate.setCreatorId(player().getObjectId());
	player().getCommonData()->setDaeva(false);
	install<GroupGateProbe>(gate).handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_GROUPGATE_BEFORE_CHANGE_CLASS())}));
	EXPECT_FALSE(player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 1)) << "no question asked";
}

/** :36-39: a Daeva who is neither the creator nor in a group has no right */
TEST_F(PortalsA1AiTest, AStrangersGroupGateCannotBeUsed) {
	Npc& gate = npcAt(833207);
	gate.setCreatorId(4242); // no such player
	player().getCommonData()->setDaeva(true);
	install<GroupGateProbe>(gate).handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_GROUPGATE_NO_RIGHT())}));
	EXPECT_FALSE(player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 1));
}

/** :40-72: the creator is asked to move (9 m); accepting Pandaemonium's gate (833207) teleports, once */
TEST_F(PortalsA1AiTest, TheCreatorIsAskedAndAcceptingTeleports) {
	playerIntoWorld();
	Npc& gate = npcAt(833207);
	gate.setCreatorId(player().getObjectId());
	player().getCommonData()->setDaeva(true);
	install<GroupGateProbe>(gate).handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_QUESTION_WINDOW(ASK_GROUP_GATE_MOVE, gate.getObjectId(), 9, std::vector<std::string>{}))}))
		<< "AIActions.addRequest(this, player, STR_ASK_GROUP_GATE_DO_YOU_ACCEPT_MOVE, CANCEL_DIALOG_METERS, request)";
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT)) << "nothing before the answer";
	// the test world has Poeta only: the teleport to Pandaemonium stops at its packet, which names the map
	expectThrowNaming([this] { player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 1); }, "120010000");
	EXPECT_FALSE(player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 1)) << "answered once";
}

/** :50-53: Sanctum's gate (833208) teleports to 110010000 */
TEST_F(PortalsA1AiTest, SanctumsGroupGateLeadsToSanctum) {
	playerIntoWorld();
	Npc& gate = npcAt(833208);
	gate.setCreatorId(player().getObjectId());
	player().getCommonData()->setDaeva(true);
	install<GroupGateProbe>(gate).handleDialogStart(player());
	expectThrowNaming([this] { player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 1); }, "110010000");
}

/** :67-70: declining does nothing */
TEST_F(PortalsA1AiTest, DecliningTheGroupGateDoesNothing) {
	playerIntoWorld();
	Npc& gate = npcAt(833208);
	gate.setCreatorId(player().getObjectId());
	player().getCommonData()->setDaeva(true);
	install<GroupGateProbe>(gate).handleDialogStart(player());
	ASSERT_TRUE(player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 0));
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT));
}

// ---- the other portals -----------------------------------------------------------------------------------------------------------------

/** The factories of the other eight markers build their classes */
TEST_F(PortalsA1AiTest, ThePortalMarkersBuildTheirClasses) {
	Npc& npc = npcAt(900100);
	EXPECT_NE(dynamic_cast<portals::DramataPortalAI*>(portals::DramataPortalAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<portals::LegionDominionPortalAI*>(portals::LegionDominionPortalAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<portals::OccupiedRentusBasePortalAI*>(portals::OccupiedRentusBasePortalAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<portals::PortalElevatorAI*>(portals::PortalElevatorAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<portals::PortalRequestAI*>(portals::PortalRequestAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<portals::SealedDanuarMysticariumPortals*>(portals::SealedDanuarMysticariumPortals_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<portals::TAGPortalAI*>(portals::TAGPortalAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<portals::abyssalsplinter::AbyssalSplinterPortalAI*>(
		          portals::abyssalsplinter::AbyssalSplinterPortalAI_aiFactory(npc).get()),
		nullptr);
}

class SplinterPortalProbe final : public portals::abyssalsplinter::AbyssalSplinterPortalAI {
public:
	using AbyssalSplinterPortalAI::AbyssalSplinterPortalAI;
	using AbyssalSplinterPortalAI::handleUseItemFinish;
};

/** AbyssalSplinterPortalAI.java:21-30: the devices at x 302.201, 334.001 and 362.192 teleport; another does nothing */
TEST_F(PortalsA1AiTest, TheSplinterDevicesTeleportByTheirPlace) {
	install<SplinterPortalProbe>(npcAt(900100, 102.5f)).handleUseItemFinish(player());
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT)) << "not one of the three";
	// the device's teleport (to its own map, the player's instance 2) stops at the test world, which has one Poeta instance
	expectThrowNaming([this] { install<SplinterPortalProbe>(npcAt(900100, 334.001f)).handleUseItemFinish(player()); }, "210010000");
	expectThrowNaming([this] { install<SplinterPortalProbe>(npcAt(900100, 302.201f)).handleUseItemFinish(player()); }, "210010000");
	expectThrowNaming([this] { install<SplinterPortalProbe>(npcAt(900100, 362.192f)).handleUseItemFinish(player()); }, "210010000");
}

class RentusPortalProbe final : public portals::OccupiedRentusBasePortalAI {
public:
	using OccupiedRentusBasePortalAI::OccupiedRentusBasePortalAI;
	using OccupiedRentusBasePortalAI::checkDialog;
};

/** OccupiedRentusBasePortalAI.java:20-36: 832991 opens to Asmodians (an Elyos gets page 10), 832992 to Elyos (1011) */
TEST_F(PortalsA1AiTest, TheRentusPortalsOpenToOneRace) {
	Npc& asmodianPortal = npcAt(832991);
	install<RentusPortalProbe>(asmodianPortal).checkDialog(player());
	Npc& elyosPortal = npcAt(832992);
	install<RentusPortalProbe>(elyosPortal).checkDialog(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_DIALOG_WINDOW(asmodianPortal.getObjectId(), 10)),
						  serializedFor(SM_DIALOG_WINDOW(elyosPortal.getObjectId(), 1011))}))
		<< "the player is an Elyos";
}

class DramataProbe final : public portals::DramataPortalAI {
public:
	using DramataPortalAI::DramataPortalAI;
	using DramataPortalAI::handleSpawned;
	using DramataPortalAI::handleUseItemFinish;
};

/** DramataPortalAI.java:26-47: spawned outside the two cities it has no target, and its use does nothing */
TEST_F(PortalsA1AiTest, ADramataRiftElsewhereLeadsNowhere) {
	DramataProbe& ai = install<DramataProbe>(npcAt(900100));
	ai.handleSpawned();
	clearSent();
	ai.handleUseItemFinish(player());
	EXPECT_EQ(sent(), exactly({}));
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT));
}

/** SealedDanuarMysticariumPortals.java:58-61: another npc outside an instance does nothing */
TEST_F(PortalsA1AiTest, ASealedMysticariumPortalOutsideItsInstanceDoesNothing) {
	Npc& portal = npcAt(731583);
	EXPECT_TRUE(install<portals::SealedDanuarMysticariumPortals>(portal).onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(sent(), exactly({}));
}

/** LegionDominionPortalAI.java:29-32: any choice but the first closes the dialog */
TEST_F(PortalsA1AiTest, TheLegionDominionPortalAnswersOnlyItsFirstChoice) {
	Npc& portal = npcAt(900100);
	EXPECT_TRUE(install<portals::LegionDominionPortalAI>(portal).onDialogSelect(player(), model::DialogAction::SETPRO2, 77, 0));
	EXPECT_EQ(sent(), exactly({serializedFor(SM_DIALOG_WINDOW(portal.getObjectId(), 0, 77))}));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
