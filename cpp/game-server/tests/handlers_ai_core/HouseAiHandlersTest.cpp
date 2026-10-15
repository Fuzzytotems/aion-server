// M5h HS-2 (m5h-plan.md 14.2, H-01, H-02, H-06, H-07): the three AIs of the house npcs - ButlerAI ("butler") and HouseSignAI ("housesign") of
// P5-05, and StudioPortalAI ("studioportal") of chunk A1 under P5-11's lease of handlers/ai/portals/StudioPortalAI.* (chunks.cmake) - and
// InstanceService::getOrCreateHouseInstance (P5-13, under P5-11's lease of InstanceService.cpp), which the studio portal calls. M5h-2 H2-3 adds
// HouseGateAI ("housegate") of chunk A1, under the same lease (handlers/ai/portals/HouseGateAI.*); H2-4 FriendPortalAI ("friendportal").
//
// Java: data/handlers/ai/ButlerAI.java:24-40, HouseSignAI.java:22-29, portals/StudioPortalAI.java:31-61, portals/HouseGateAI.java:32-107;
// InstanceService.java:127-135.
//
// LINK WORKAROUND (the one TravelAiHandlersTest.cpp describes): A1's handler library is not linked into this executable, so this file compiles
// the leased source itself; it is the only translation unit that does.
#include "aion/gameserver/handlers/ai/portals/StudioPortalAI.cpp"
#include "aion/gameserver/handlers/ai/portals/HouseGateAI.cpp"
#include "aion/gameserver/handlers/ai/portals/FriendPortalAI.cpp"

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"
#include "../dao/DaoTestDatabase.h"

// The fixture is TravelAiHandlersTest's (the player of tests/cm_ak/ItemPacketTestSupport.h, an Elyos, moved into a Poeta map instance of its own),
// with no database for the butler: HouseScriptsDAO.getPlayerScripts then logs its SQLException and answers the 8 empty slots
// (HouseScriptsDAO.cpp:44-58), which is what a house without stored scripts answers. The studio portal's cases need HousingService, whose start
// reads the database (HousesDAO.loadHouses, PlayerDAO.getUsedIDs): they use the test database of the DAO tests, as TravelAiHandlersTest's bind
// case does, and skip without it (ctest runs every case in its own process; the HousingService of the process starts on that database, empty).
// The rows, verbatim with file:line:
// - npc_templates.xml:530145-530151 (810021 studio butler, ai butler), :530021-530025 (810007 resident information, ai housesign) and
//   :454683-454687 (730517 studio entrance, ai studioportal);
// - houses.xml:617-627, the Elyos studio land 329001 (address 2001 on 720010000 with its exit to 700010000), its building given the type the
//   building data gives it (PERSONAL_INS), and the house land of tests/objects/SummonedObjectsTest.cpp (address 10001, PERSONAL_FIELD).
//
// Not covered here, said in place: leaving a studio that exists (World has no maps in this executable: World.getWorldMap(exit map) is the
// gate's, m5h C19) and a studio without a position (InstanceService.getOrCreatePersonalInstance spawns the map's instance: the gate's C15).
// ActionItemNpcAI's use bar before handleUseItemFinish is ActionItemNpcAiTest.cpp's.
// The house gate's cases put the gate's creator (the player, or a second player "Buddy") into World, as Npc.getCreator finds him there. Not
// covered here: the group-member arm of the creator check (no PlayerGroupService in this executable; the gate's), and a zone that forbids the
// return to battle (no zones in this executable: the DONT_RETURN question and setBattleReturnCoords(0, null) are the gate's).

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/ai/ButlerAI.h"
#include "aion/gameserver/handlers/ai/HouseSignAI.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/SummonedHouseNpc.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseDoorStateInfo.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_TELEPORT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_LOC.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

// The factory functions the AION_AI markers of ButlerAI.cpp and HouseSignAI.cpp define (this executable links P5-05's library), declared exactly
// as Registry.ai.gen.cpp declares them
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory ButlerAI_aiFactory;
::aion::gameserver::handlers::AIFactory HouseSignAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace roots = gameserver::handlers::ai;
namespace portals = gameserver::handlers::ai::portals;

using model::gameobjects::Npc;
using model::house::House;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_HOUSE_TELEPORT;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::aion::serverpackets::SM_TELEPORT_LOC;

constexpr int32_t BUTLER = 810021;
constexpr int32_t RESIDENT_INFORMATION = 810007;
constexpr int32_t STUDIO_ENTRANCE = 730517;
constexpr int32_t HOUSE_GATE = 730517; // a house gate's own row is no matter here: the AI is installed on the npc
constexpr int32_t ASK_GROUP_GATE_MOVE = 160014; // SM_QUESTION_WINDOW.STR_ASK_GROUP_GATE_DO_YOU_ACCEPT_MOVE
constexpr int32_t BUDDY_ID = 710102;
constexpr int32_t HOUSE_ADDRESS = 10001;
constexpr int32_t STUDIO_ADDRESS = 2001;
constexpr int32_t HOUSING_KICK = 86;	   // DialogAction.java:101 -> DialogPage.HOUSING_KICK, page 40 (DialogPage.java:55)
constexpr int32_t HOUSING_PAY_RENT = 85; // DialogAction.java:100 -> DialogPage.HOUSING_PAY_RENT, page 39 (DialogPage.java:54)
constexpr int32_t NO_PAGE_ACTION = 99999;
constexpr int32_t SM_HOUSE_SCRIPTS_OPCODE = 131; // ServerPacketsOpcodes.java:149

constexpr const char* NPC_ROWS = R"(<npc_templates>)"
	// npc_templates.xml:530145-530151
	R"(<npc_template npc_id="810021" level="1" name="studio butler" name_id="461900" height="1.16875" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="GENERAL" type="HOUSING" ai="butler" sangle="0" attack_speed="2000" hpgauge="3">)"
	R"(<stats maxHp="2256"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" /></stats>)"
	R"(<bound_radius front="0.595" side="0.3774" upper="1.16875" />)"
	R"(<talk_info distance="2" is_dialog="true" func_dialogs="88 86 98" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:530021-530025
	R"(<npc_template npc_id="810007" level="1" name="&lt;resident information&gt;" name_id="461466" height="2" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="GENERAL" type="HOUSING" ai="housesign" sangle="0" attack_speed="2000" hpgauge="3">)"
	R"(<stats maxHp="172" /><bound_radius front="0.25" side="0.35" upper="3.2" />)"
	R"(<talk_info distance="1" is_dialog="true" func_dialogs="88 99 84 97 87" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:454683-454687
	R"(<npc_template npc_id="730517" level="1" name="studio entrance" name_id="461469" height="2" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="studioportal" sangle="0" attack_speed="2000" hpgauge="3">)"
	R"(<stats maxHp="172" /><bound_radius front="0.25" side="0.35" upper="3.2" />)"
	R"(<talk_info distance="5" delay="2" is_dialog="true" func_dialogs="83" can_talk_invisible="false" /></npc_template>)"
	R"(</npc_templates>)";

constexpr const char* HOUSE_LANDS = R"(<house_lands>)"
	// tests/objects/SummonedObjectsTest.cpp's land: one PERSONAL_FIELD address
	R"(<land id="325001" teleport_npc="810003" manager_npc="810017" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">)"
	R"(<addresses><address id="10001" map="700010000" town="1001" x="696.159973" y="1999.969971" z="174.42577"/></addresses>)"
	R"(<buildings><building id="350000" default="true" type="PERSONAL_FIELD" size="HOUSE"/></buildings>)"
	R"(<sale level="50" gold_price="1000000000" point_price="0"/><fee>20000000</fee><caps room="false" floor="false" emblemId="2" addon="true"/>)"
	R"(</land>)"
	// houses.xml:617-627, the building's type and size as house_buildings.xml:99 gives them
	R"(<land id="329001" teleport_npc="810003" manager_npc="810021" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">)"
	R"(<addresses><address id="2001" map="720010000" town="0" x="366.242615" y="295.776093" z="222.3536" exit_map="700010000" exit_x="2573.0" exit_y="1961.0" exit_z="185.0"/></addresses>)"
	R"(<buildings><building id="355000" default="true" type="PERSONAL_INS" size="STUDIO"/></buildings>)"
	R"(<sale level="21" gold_price="4000000" point_price="0"/><fee>0</fee><caps room="true" floor="true" emblemId="0" addon="false"/>)"
	R"(</land>)"
	R"(</house_lands>)";

class HouseSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	HouseSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** ButlerAI with its protected hook callable */
class ButlerAiProbe final : public roots::ButlerAI {
public:
	using ButlerAI::ButlerAI;
	using ButlerAI::handleCreatureSee;
};

/** HouseGateAI with its protected hook callable */
class HouseGateAiProbe final : public portals::HouseGateAI {
public:
	using HouseGateAI::HouseGateAI;
	using HouseGateAI::handleDialogStart;
};

/** FriendPortalAI with its protected hook callable */
class FriendPortalAiProbe final : public portals::FriendPortalAI {
public:
	using FriendPortalAI::FriendPortalAI;
	using FriendPortalAI::handleDialogStart;
};

/** StudioPortalAI with its protected hook callable */
class StudioPortalAiProbe final : public portals::StudioPortalAI {
public:
	using StudioPortalAI::StudioPortalAI;
	using StudioPortalAI::handleUseItemFinish;
};

class HouseAiHandlersTest : public ItemPacketTest {
protected:
	void SetUp() override {
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		useInstance = world::WorldMap2DInstance::create(*map, 2, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
		standIn(player());
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcContext, NPC_ROWS));
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(houseContext, HOUSE_LANDS));
		clearSent();
	}

	void TearDown() override {
		for (runtime::Ptr<House>* owned : {&studio, &buddyStudio}) {
			if (*owned) { // HousingService is the process's: the studio goes as Java's changeOwner(studio, 0) removes it
				(*owned)->updateSpawn(model::templates::spawns::SpawnType::TELEPORT, nullptr);
				(*owned)->setPosition(nullptr);
				services::HousingService::getInstance().changeOwner(**owned, 0);
				*owned = nullptr;
			}
		}
		if (playerInWorld)
			world::World::getInstance().removeObject(player());
		if (buddy.player) {
			world::World::getInstance().removeObject(*buddy.player);
			buddy.player->setClientConnection(nullptr);
		}
		buddyClient.reset();
		buddy = {};
		npcs.clear();
		houses.clear();
		spawnGroups.clear();
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		useInstance = nullptr;
		studioInstance = nullptr;
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	void standIn(model::gameobjects::player::Player& user) {
		user.setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));
		user.getPosition()->setIsSpawned(true);
	}

	model::templates::spawns::SpawnTemplate& spawnOf(int32_t npcId, float x, float y = 100.0f) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<HouseSpawnTemplate>(*group, x, y, 50.0f));
		spawnGroups.push_back(group);
		return spawn;
	}

	void place(Npc& npc, float x) {
		npc.setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(x, 100.0f, 50.0f)));
		npc.getPosition()->setIsSpawned(true);
	}

	/** A plain npc of the row 2 m from the player (Java VisibleObjectSpawner.spawnNpc) */
	Npc& npcAt(int32_t npcId) {
		model::templates::spawns::SpawnTemplate& spawn = spawnOf(npcId, 102.0f);
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, 102.0f);
		npcs.push_back(npc);
		return *npc;
	}

	/** The house of address 10001 (no owner) */
	House& house() {
		runtime::Ref<House> h = model::gameobjects::VisibleObject::create<House>(dataholders::DataManager::HOUSE_DATA->getAddress(HOUSE_ADDRESS), 0);
		houses.push_back(h);
		return *h;
	}

	/** A house npc of the row, built by its house as HousingService.spawnHouses builds it (SummonedHouseNpc) */
	Npc& houseNpc(int32_t npcId, House& creator) {
		model::templates::spawns::SpawnTemplate& spawn = spawnOf(npcId, 102.0f);
		runtime::Ref<model::gameobjects::SummonedHouseNpc> npc =
			model::gameobjects::VisibleObject::create<model::gameobjects::SummonedHouseNpc>(std::make_unique<controllers::NpcController>(), spawn, creator);
		place(*npc, 102.0f);
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

	/** The DAO tests' database, emptied, with the player's row; DatabaseFactory initialized for the process */
	void housingDatabase() {
		dao::test::setUpDatabaseOnce();
		dao::test::clearTables();
		dao::test::insertPlayer(player().getObjectId(), "Holder", 9901);
	}

	/** The player in World: Npc.getCreator finds the gate's creator there */
	void playerIntoWorld() {
		world::World::getInstance().storeObject(player());
		playerInWorld = true;
	}

	/** A second player "Buddy" (an Elyos) in World, out of the player's sight */
	model::gameobjects::player::Player& addBuddy() {
		buddy = makePlayer(BUDDY_ID, 9902, "Buddy");
		buddyClient = std::make_unique<TestClient>();
		buddyClient->enterWorld(buddy);
		world::World::getInstance().storeObject(*buddy.player);
		return *buddy.player;
	}

	/** A house gate 2 m from the player, created by `creatorId` (Java: a summoned npc's creator) */
	HouseGateAiProbe& gateOf(int32_t creatorId) {
		Npc& gate = npcAt(HOUSE_GATE);
		gate.setCreatorId(creatorId);
		return install<HouseGateAiProbe>(gate);
	}

	/**
	 * The player's own studio (address 2001), spawned in the fixture's instance and with its relationship crystal 10 m north of its address
	 * point - the studio of EnteringTheOwnStudioTeleportsToItsAddressWithTheBeam: getTeleportHeading 30
	 */
	House& ownStudio() {
		services::HousingService::getInstance().registerPlayerStudio(player());
		studio = services::HousingService::getInstance().getPlayerStudio(player().getObjectId());
		studio->setPosition(world::WorldPosition::create(720010000, 366.242615f, 295.776093f, 222.3536f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));
		model::templates::spawns::SpawnTemplate& crystalSpawn = spawnOf(RESIDENT_INFORMATION, 366.242615f, 305.776093f);
		runtime::Ref<Npc> crystal = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), crystalSpawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(RESIDENT_INFORMATION));
		npcs.push_back(crystal);
		studio->updateSpawn(model::templates::spawns::SpawnType::TELEPORT, crystal);
		clearSent();
		return *studio;
	}

	std::vector<uint8_t> page(Npc& npc, int32_t pageId) { return serializedFor(SM_DIALOG_WINDOW(npc.getObjectId(), pageId)); }

	/** SM_HOUSE_SCRIPTS.writeImpl of 8 empty slots: D(address), H(8), per slot C(id) H(0) (SM_HOUSE_SCRIPTS.java) */
	static std::vector<uint8_t> emptyScripts(int32_t address) {
		PacketWriter body;
		body.D(address).H(8);
		for (int32_t id = 0; id < 8; id++)
			body.C(id).H(0);
		return javaPacket(SM_HOUSE_SCRIPTS_OPCODE, body);
	}

	xml::LoadContext npcContext;
	xml::LoadContext houseContext;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	runtime::Ref<world::WorldMapInstance> useInstance;
	runtime::Ptr<world::WorldMapInstance> studioInstance;
	runtime::Ptr<House> studio;
	runtime::Ptr<House> buddyStudio;
	bool playerInWorld = false;
	PlayerFixture buddy;
	std::unique_ptr<TestClient> buddyClient;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
	std::vector<runtime::Ref<House>> houses;
};

// ---- ButlerAI (ButlerAI.java:24-40) --------------------------------------------------------------------------------------------------------

TEST_F(HouseAiHandlersTest, TheButlerMarkerBuildsAButler) {
	Npc& butler = houseNpc(BUTLER, house());
	std::unique_ptr<gameserver::ai::AbstractAI> ai = roots::ButlerAI_aiFactory(butler);
	EXPECT_NE(dynamic_cast<roots::ButlerAI*>(ai.get()), nullptr) << "the factory of the \"butler\" marker";
	ai = roots::HouseSignAI_aiFactory(butler);
	EXPECT_NE(dynamic_cast<roots::HouseSignAI*>(ai.get()), nullptr) << "the factory of the \"housesign\" marker";
}

TEST_F(HouseAiHandlersTest, AButlerActionOpensItsPage) {
	Npc& butler = houseNpc(BUTLER, house());
	ButlerAiProbe& ai = install<ButlerAiProbe>(butler);

	EXPECT_TRUE(ai.onDialogSelect(player(), HOUSING_KICK, 0, 0));

	EXPECT_EQ(sent(), exactly({page(butler, 40)})) << "kickDialog: SM_DIALOG_WINDOW(owner, DialogPage.getByActionId(86).id())";
}

TEST_F(HouseAiHandlersTest, AButlerActionWithoutPageIsNotHandled) {
	Npc& butler = houseNpc(BUTLER, house());
	ButlerAiProbe& ai = install<ButlerAiProbe>(butler);

	EXPECT_FALSE(ai.onDialogSelect(player(), NO_PAGE_ACTION, 0, 0)) << "DialogPage.NULL: kickDialog returns false";

	EXPECT_EQ(sent(), exactly({}));
}

TEST_F(HouseAiHandlersTest, AButlerSendsItsHousesScriptsToAPlayerItSees) {
	House& h = house();
	Npc& butler = houseNpc(BUTLER, h);
	ButlerAiProbe& ai = install<ButlerAiProbe>(butler);
	ASSERT_TRUE(h.getPlayerScripts()) << "loaded: no database, so the 8 empty slots";
	clearSent();

	ai.handleCreatureSee(player());

	EXPECT_EQ(sent(), exactly({emptyScripts(HOUSE_ADDRESS)})) << "House.sendScripts -> PlayerScripts.sendToPlayer(player, address id)";
}

TEST_F(HouseAiHandlersTest, AButlerOfAHouseWhoseScriptsAreNotLoadedSendsNothing) {
	Npc& butler = houseNpc(BUTLER, house());
	ButlerAiProbe& ai = install<ButlerAiProbe>(butler);

	ai.handleCreatureSee(player());

	EXPECT_EQ(sent(), exactly({})) << "House.sendScripts: playerScripts == null returns";
}

TEST_F(HouseAiHandlersTest, AButlerSeeingAnNpcSendsNothing) {
	House& h = house();
	Npc& butler = houseNpc(BUTLER, h);
	ButlerAiProbe& ai = install<ButlerAiProbe>(butler);
	ASSERT_TRUE(h.getPlayerScripts());
	Npc& other = npcAt(RESIDENT_INFORMATION);
	clearSent();

	ai.handleCreatureSee(other);

	EXPECT_EQ(sent(), exactly({})) << "creature instanceof Player fails";
}

TEST_F(HouseAiHandlersTest, AButlerWithoutHouseSendsNothing) {
	Npc& butler = npcAt(BUTLER); // a plain npc: no creator
	ButlerAiProbe& ai = install<ButlerAiProbe>(butler);

	ai.handleCreatureSee(player());

	EXPECT_EQ(sent(), exactly({})) << "getCreator() instanceof House fails";
}

// ---- HouseSignAI (HouseSignAI.java:22-29) --------------------------------------------------------------------------------------------------

TEST_F(HouseAiHandlersTest, ASignActionOpensItsPage) {
	Npc& sign = houseNpc(RESIDENT_INFORMATION, house());
	roots::HouseSignAI& ai = install<roots::HouseSignAI>(sign);

	EXPECT_TRUE(ai.onDialogSelect(player(), HOUSING_PAY_RENT, 0, 0));

	EXPECT_EQ(sent(), exactly({page(sign, 39)}));
}

TEST_F(HouseAiHandlersTest, ASignActionWithoutPageIsNotHandled) {
	Npc& sign = houseNpc(RESIDENT_INFORMATION, house());
	roots::HouseSignAI& ai = install<roots::HouseSignAI>(sign);

	EXPECT_FALSE(ai.onDialogSelect(player(), NO_PAGE_ACTION, 0, 0));

	EXPECT_EQ(sent(), exactly({}));
}

// ---- StudioPortalAI (StudioPortalAI.java:31-61) --------------------------------------------------------------------------------------------

TEST_F(HouseAiHandlersTest, TheStudioPortalMarkerBuildsAStudioPortal) {
	Npc& portal = npcAt(STUDIO_ENTRANCE);
	std::unique_ptr<gameserver::ai::AbstractAI> ai = portals::StudioPortalAI_aiFactory(portal);
	EXPECT_NE(dynamic_cast<portals::StudioPortalAI*>(ai.get()), nullptr) << "the factory of the \"studioportal\" marker";
}

TEST_F(HouseAiHandlersTest, AStudioPortalDialogActionIsHandledSilently) {
	Npc& portal = npcAt(STUDIO_ENTRANCE);
	StudioPortalAiProbe& ai = install<StudioPortalAiProbe>(portal);

	EXPECT_TRUE(ai.onDialogSelect(player(), HOUSING_KICK, 0, 0));

	EXPECT_EQ(sent(), exactly({}));
}

TEST_F(HouseAiHandlersTest, EnteringWithoutStudioIsRefused) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	Npc& portal = npcAt(STUDIO_ENTRANCE);
	StudioPortalAiProbe& ai = install<StudioPortalAiProbe>(portal);

	ai.handleUseItemFinish(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_HOUSING_ENTER_NEED_HOUSE())}));
	EXPECT_EQ(player().getPosition()->getWorldMapInstance().get(), useInstance.get()) << "no teleport";
}

TEST_F(HouseAiHandlersTest, EnteringTheOwnStudioTeleportsToItsAddressWithTheBeam) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	Npc& portal = npcAt(STUDIO_ENTRANCE);
	StudioPortalAiProbe& ai = install<StudioPortalAiProbe>(portal);
	services::HousingService::getInstance().registerPlayerStudio(player());
	studio = services::HousingService::getInstance().getPlayerStudio(player().getObjectId());
	ASSERT_TRUE(studio) << "registerPlayerStudio: the Elyos studio, address 2001";
	ASSERT_EQ(studio->getAddress()->getId(), STUDIO_ADDRESS);
	// the studio spawned in the fixture's instance: getOrCreateHouseInstance answers its position's instance
	studio->setPosition(world::WorldPosition::create(720010000, 366.242615f, 295.776093f, 222.3536f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));
	// and its relationship crystal (the TELEPORT spawn, House.getRelationshipCrystal) 10 m north of the address point: House.getTeleportHeading
	// is the heading towards it, 90 degrees = heading 30 (PositionUtil.convertAngleToHeading)
	model::templates::spawns::SpawnTemplate& crystalSpawn = spawnOf(RESIDENT_INFORMATION, 366.242615f, 305.776093f);
	runtime::Ref<Npc> crystal = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), crystalSpawn,
		dataholders::DataManager::NPC_DATA->getNpcTemplate(RESIDENT_INFORMATION));
	npcs.push_back(crystal);
	studio->updateSpawn(model::templates::spawns::SpawnType::TELEPORT, crystal);
	ASSERT_EQ(studio->getTeleportHeading(), 30);
	clearSent();

	ai.handleUseItemFinish(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_TELEPORT_LOC(210010000, 2, 366.242615f, 295.776093f, 222.3536f, int8_t{30},
						  model::animations::TeleportAnimation::FADE_OUT_BEAM))}))
		<< "TeleportService.teleportTo(player, the studio's instance, the address point, getTeleportHeading(), FADE_OUT_BEAM)";
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::TELEPORT)) << "the spawn waits for CM_TELEPORT_ANIMATION_DONE";
}

TEST_F(HouseAiHandlersTest, LeavingAStudioWithoutOwnerStudioDoesNothing) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	Npc& portal = npcAt(STUDIO_ENTRANCE);
	StudioPortalAiProbe& ai = install<StudioPortalAiProbe>(portal);
	// a studio map instance of an owner who has no studio ("custom spawned by admin", StudioPortalAI.java:42-43)
	studioInstance = world::WorldMap2DInstance::create(*map, 3, 4242, 0, [](world::WorldMapInstance& instance) {
		return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
			::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
	});
	player().setPosition(world::WorldPosition::create(720010000, 100.0f, 100.0f, 50.0f, int8_t{0}, studioInstance->getRegion(100.0f, 100.0f, 50.0f)));
	player().getPosition()->setIsSpawned(true);

	ai.handleUseItemFinish(player());

	EXPECT_EQ(sent(), exactly({})) << "the leaving arm (HOUSING_IDLF_PERSONAL): getPlayerStudio(4242) is null - not the entering arm's refusal";
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT));
}

// ---- HouseGateAI (HouseGateAI.java:32-107) --------------------------------------------------------------------------------------------------

/** AdminConfig.HOUSE_ENTER_ALL for one case: 3 makes a player (access level 0) no GM, so the door applies (House.canEnter) */
class EnterAllScope {
public:
	explicit EnterAllScope(int8_t value) : saved(configs::administration::AdminConfig::HOUSE_ENTER_ALL.load()) {
		configs::administration::AdminConfig::HOUSE_ENTER_ALL.store(value);
	}
	~EnterAllScope() { configs::administration::AdminConfig::HOUSE_ENTER_ALL.store(saved); }
	EnterAllScope(const EnterAllScope&) = delete;
	EnterAllScope& operator=(const EnterAllScope&) = delete;

private:
	int8_t saved;
};

TEST_F(HouseAiHandlersTest, TheHouseGateMarkerBuildsAHouseGate) {
	Npc& gate = npcAt(HOUSE_GATE);
	std::unique_ptr<gameserver::ai::AbstractAI> ai = portals::HouseGateAI_aiFactory(gate);
	EXPECT_NE(dynamic_cast<portals::HouseGateAI*>(ai.get()), nullptr) << "the factory of the \"housegate\" marker";
}

TEST_F(HouseAiHandlersTest, AStrangersHouseGateCannotBeUsed) {
	HouseGateAiProbe& ai = gateOf(4242); // no such player: neither the creator, nor a friend, nor a group member

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_HOUSING_TELEPORT_CANT_USE())}));
	EXPECT_FALSE(player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 1)) << "no question asked";
}

TEST_F(HouseAiHandlersTest, ACreatorWithoutHouseIsToldHeNeedsOne) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	playerIntoWorld();
	HouseGateAiProbe& ai = gateOf(player().getObjectId());

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_HOUSING_TELEPORT_NEED_HOUSE())}))
		<< "player.equals(getCreator()), findActiveHouse null, the creator himself";
}

TEST_F(HouseAiHandlersTest, AFriendOfACreatorWithoutHouseCannotUseTheGate) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	model::gameobjects::player::Player& creator = addBuddy();
	player().getFriendList().addFriend(*model::gameobjects::player::Friend::create(*creator.getCommonData(), ""));
	HouseGateAiProbe& ai = gateOf(BUDDY_ID);
	clearSent();

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_HOUSING_TELEPORT_CANT_USE())})) << "findActiveHouse null, not the creator";
}

TEST_F(HouseAiHandlersTest, AFriendWhoIsNoDaevaHasNoRightToEnter) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	model::gameobjects::player::Player& creator = addBuddy();
	player().getFriendList().addFriend(*model::gameobjects::player::Friend::create(*creator.getCommonData(), ""));
	services::HousingService::getInstance().registerPlayerStudio(creator);
	buddyStudio = services::HousingService::getInstance().getPlayerStudio(BUDDY_ID);
	ASSERT_TRUE(buddyStudio);
	player().getCommonData()->setDaeva(false);
	HouseGateAiProbe& ai = gateOf(BUDDY_ID);
	clearSent();

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_ENTER_NO_RIGHT(STUDIO_ADDRESS))}))
		<< "the friend arm passes, the creator's studio is found, !isDaeva()";
}

TEST_F(HouseAiHandlersTest, AFriendBehindAClosedDoorHasNoRightToEnter) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	model::gameobjects::player::Player& creator = addBuddy();
	player().getFriendList().addFriend(*model::gameobjects::player::Friend::create(*creator.getCommonData(), ""));
	services::HousingService::getInstance().registerPlayerStudio(creator);
	buddyStudio = services::HousingService::getInstance().getPlayerStudio(BUDDY_ID);
	ASSERT_TRUE(buddyStudio);
	buddyStudio->setDoorState(model::house::HouseDoorState::CLOSED);
	EnterAllScope enterAll(int8_t{3});
	player().getCommonData()->setDaeva(true);
	HouseGateAiProbe& ai = gateOf(BUDDY_ID);
	clearSent();

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_ENTER_NO_RIGHT2())})) << "!house.canEnter(player)";
	EXPECT_FALSE(player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 1));
}

TEST_F(HouseAiHandlersTest, TheDaevaCreatorIsAskedToMoveIntoHisHouse) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	playerIntoWorld();
	player().getCommonData()->setDaeva(true);
	ownStudio();
	HouseGateAiProbe& ai = gateOf(player().getObjectId());

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_QUESTION_WINDOW(ASK_GROUP_GATE_MOVE, ai.getOwner().getObjectId(), 9, std::vector<std::string>{}))}))
		<< "no zone forbids the return: STR_ASK_GROUP_GATE_DO_YOU_ACCEPT_MOVE, AIActions.addRequest(this, player, requestId, 9, request)";
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT)) << "nothing before the answer";
}

TEST_F(HouseAiHandlersTest, AcceptingTheGateTeleportsIntoTheHouseAndKeepsTheWayBack) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	playerIntoWorld();
	player().getCommonData()->setDaeva(true);
	ASSERT_EQ(ownStudio().getTeleportHeading(), 30);
	HouseGateAiProbe& ai = gateOf(player().getObjectId());
	ai.handleDialogStart(player());
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 1));

	EXPECT_EQ(sent(), exactly({serializedFor(SM_HOUSE_TELEPORT(STUDIO_ADDRESS, player().getObjectId())),
						  serializedFor(SM_TELEPORT_LOC(210010000, 2, 366.242615f, 295.776093f, 222.3536f, int8_t{30},
							  model::animations::TeleportAnimation::JUMP_IN_GATE))}))
		<< "SM_HOUSE_TELEPORT(address, responder), then teleportTo(the house's instance, the address point, getTeleportHeading(), JUMP_IN_GATE)";
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::TELEPORT));
	EXPECT_EQ(player().getBattleReturnMap(), 210010000) << "setBattleReturnCoords(responder.getWorldId(), ...)";
	runtime::Ptr<runtime::Array<float>> coords = player().getBattleReturnCoords();
	ASSERT_TRUE(coords);
	ASSERT_EQ(coords->length(), 3);
	EXPECT_EQ((*coords)[0], 100.0f);
	EXPECT_EQ((*coords)[1], 100.0f);
	EXPECT_EQ((*coords)[2], 50.0f);
}

TEST_F(HouseAiHandlersTest, DecliningTheGateDoesNothing) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: HousingService starts from the database";
	housingDatabase();
	playerIntoWorld();
	player().getCommonData()->setDaeva(true);
	ownStudio();
	HouseGateAiProbe& ai = gateOf(player().getObjectId());
	ai.handleDialogStart(player());
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(ASK_GROUP_GATE_MOVE, 0));

	EXPECT_EQ(sent(), exactly({})) << "denyRequest only decides";
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT));
	EXPECT_EQ(player().getBattleReturnMap(), 0);
	EXPECT_FALSE(player().getBattleReturnCoords());
}

// ---- FriendPortalAI (FriendPortalAI.java:23-41) ---------------------------------------------------------------------------------------------

constexpr int32_t HOUSING_FRIENDLIST_PAGE = 36;  // DialogPage.HOUSING_FRIENDLIST.id() (DialogPage.java)

TEST_F(HouseAiHandlersTest, TheFriendPortalMarkerBuildsAFriendPortal) {
	Npc& crystal = houseNpc(RESIDENT_INFORMATION, house());
	std::unique_ptr<gameserver::ai::AbstractAI> ai = portals::FriendPortalAI_aiFactory(crystal);
	EXPECT_NE(dynamic_cast<portals::FriendPortalAI*>(ai.get()), nullptr) << "the factory of the \"friendportal\" marker";
}

TEST_F(HouseAiHandlersTest, TheOwnerOfTheCrystalsHouseGetsTheFriendListPage) {
	House& h = house();
	h.setOwnerId(player().getObjectId());
	Npc& crystal = houseNpc(RESIDENT_INFORMATION, h);
	FriendPortalAiProbe& ai = install<FriendPortalAiProbe>(crystal);

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(network::aion::serverpackets::SM_FRIEND_LIST()), page(crystal, HOUSING_FRIENDLIST_PAGE)}));
}

TEST_F(HouseAiHandlersTest, ACrystalOfAHouseWithoutOwnerIsOpenToAll) {
	Npc& crystal = houseNpc(RESIDENT_INFORMATION, house()); // owner 0
	FriendPortalAiProbe& ai = install<FriendPortalAiProbe>(crystal);

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(network::aion::serverpackets::SM_FRIEND_LIST()), page(crystal, HOUSING_FRIENDLIST_PAGE)}))
		<< "playerOwner == 0";
}

TEST_F(HouseAiHandlersTest, AFriendOfTheOwnerGetsTheFriendListPage) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: the second player's enter world starts HousingService";
	housingDatabase();
	model::gameobjects::player::Player& owner = addBuddy();
	player().getFriendList().addFriend(*model::gameobjects::player::Friend::create(*owner.getCommonData(), ""));
	House& h = house();
	h.setOwnerId(BUDDY_ID);
	Npc& crystal = houseNpc(RESIDENT_INFORMATION, h);
	FriendPortalAiProbe& ai = install<FriendPortalAiProbe>(crystal);
	clearSent();

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(network::aion::serverpackets::SM_FRIEND_LIST()), page(crystal, HOUSING_FRIENDLIST_PAGE)}));
}

TEST_F(HouseAiHandlersTest, AStrangerToTheOwnerCannotUseTheCrystal) {
	House& h = house();
	h.setOwnerId(4242);
	Npc& crystal = houseNpc(RESIDENT_INFORMATION, h);
	FriendPortalAiProbe& ai = install<FriendPortalAiProbe>(crystal);

	ai.handleDialogStart(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_HOUSING_TELEPORT_CANT_USE())})) << "no friend, no legion member";
}

TEST_F(HouseAiHandlersTest, AFriendPortalOnAPlainNpcIsAClassCast) {
	Npc& plain = npcAt(RESIDENT_INFORMATION); // not a SummonedHouseNpc
	FriendPortalAiProbe& ai = install<FriendPortalAiProbe>(plain);

	EXPECT_THROW(ai.handleDialogStart(player()), runtime::ClassCastException) << "Java: (SummonedHouseNpc) getOwner()";
}

// ---- InstanceService.getOrCreateHouseInstance (InstanceService.java:127-135) ---------------------------------------------------------------

TEST_F(HouseAiHandlersTest, AHouseWithAPositionIsInItsPositionsInstance) {
	House& h = house();
	h.setPosition(world::WorldPosition::create(700010000, 696.159973f, 1999.969971f, 174.42577f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));

	runtime::Ptr<world::WorldMapInstance> instance = services::instance::InstanceService::getOrCreateHouseInstance(h);

	EXPECT_EQ(instance.get(), useInstance.get());
	h.setPosition(nullptr);
}

TEST_F(HouseAiHandlersTest, AHouseWithoutPositionThatIsNoStudioHasNoInstance) {
	House& h = house(); // PERSONAL_FIELD, never spawned

	try {
		services::instance::InstanceService::getOrCreateHouseInstance(h);
		FAIL() << "Java: throw new NullPointerException(house + \" has no instance\")";
	} catch (const runtime::NullPointerException& e) {
		EXPECT_EQ(std::string(e.what()), h.toString() + " has no instance");
	}
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
