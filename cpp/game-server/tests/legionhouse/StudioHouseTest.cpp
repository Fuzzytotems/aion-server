// M5h H-07 (m5h-plan.md 14.2, HS-4 part 3): the studio's unit cases the gate cannot reach - the studio of each race (HouseData.getStudioAddress,
// the Asmodian 3001 included), HouseObjectFactory over every kind HousingObjectData binds and from an item (the use_days expiry; the item
// without a house object; the bare <housedeco/> whose DecorateAction.getTemplateId is 0), the PlayerRegisteredItemsDAO store -> load round trip
// through createNew (the restart path, HouseRegistry.save then House.reloadHouseRegistry), and HouseController.onDespawn of a reusable studio.
// On the test database (LegionHouseTestSupport.h, run under gate_lock): HousingService's start and the DAOs read and write it.
//
// The rows are verbatim with file:line: housing/houses.xml:617-627 (land 329001, the Elyos studio 2001) and :1242-1252 (land 339001, the
// Asmodian studio 3001), their building typed as house_buildings.xml:99 types it; housing/housing_objects.xml for each kind (the line at each
// row); items/item_templates.xml:867077-867083 (the cake), :864506-864511 (the bed), :861910-861915 (the bare roof decoration);
// housing/house_parts.xml:179 (the wallpaper part). housing_objects.xml has no <jukebox> and no <move_item> row: those two are synthetic rows of
// the binding's own element names (HousingObjectData.java:20-25).
//
// Not here: UseableItemObject.onUse (its timed use, cooldown, the COOKING limit and the cancel) is the gate's C17, and the decoration mode and
// the destroyed studio's save on despawn the gate's C16 and C19 (gs.scenario.m5h). HouseObject.getPlacementLimit(bool) stays unported: Java
// never calls it (m5h-plan.md H-05).

#include "../cm_ak/ItemPacketTestSupport.h"
#include "LegionHouseTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseBuildingData.bind.h"
#include "aion/gameserver/dataholders/HouseBuildingData.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/HousePartsData.bind.h"
#include "aion/gameserver/dataholders/HousePartsData.h"
#include "aion/gameserver/dataholders/HousingObjectData.bind.h"
#include "aion/gameserver/dataholders/HousingObjectData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/ChairObject.h"
#include "aion/gameserver/model/gameobjects/EmblemObject.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/JukeBoxObject.h"
#include "aion/gameserver/model/gameobjects/MoveableObject.h"
#include "aion/gameserver/model/gameobjects/NpcObject.h"
#include "aion/gameserver/model/gameobjects/PassiveObject.h"
#include "aion/gameserver/model/gameobjects/PictureObject.h"
#include "aion/gameserver/model/gameobjects/PostboxObject.h"
#include "aion/gameserver/model/gameobjects/StorageObject.h"
#include "aion/gameserver/model/gameobjects/UseableItemObject.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/PartType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/DecorateAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/item/HouseObjectFactory.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

namespace lh = ::aion::gameserver::legionhouse::test;
namespace go = model::gameobjects;
using model::house::House;
using model::house::HouseRegistry;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::item::HouseObjectFactory;

constexpr const char* STUDIO_LANDS = R"(<house_lands>)"
	// houses.xml:617-627
	R"(<land id="329001" teleport_npc="810003" manager_npc="810021" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">)"
	R"(<addresses><address id="2001" map="720010000" town="0" x="366.242615" y="295.776093" z="222.3536" exit_map="700010000" exit_x="2573.0" exit_y="1961.0" exit_z="185.0"/></addresses>)"
	R"(<buildings><building id="355000" default="true" type="PERSONAL_INS" size="STUDIO"/></buildings>)"
	R"(<sale level="21" gold_price="4000000" point_price="0"/><fee>0</fee><caps room="true" floor="true" emblemId="0" addon="false"/></land>)"
	// houses.xml:1242-1252
	R"(<land id="339001" teleport_npc="810031" manager_npc="810026" sign_home="810030" sign_waiting="810029" sign_sale="810028" sign_nosale="810027">)"
	R"(<addresses><address id="3001" map="730010000" town="0" x="122.268707" y="358.312622" z="267.89127" exit_map="710010000" exit_x="1197.0" exit_y="2773.0" exit_z="236.0"/></addresses>)"
	R"(<buildings><building id="355000" default="true" type="PERSONAL_INS" size="STUDIO"/></buildings>)"
	R"(<sale level="21" gold_price="4000000" point_price="0"/><fee>0</fee><caps room="true" floor="true" emblemId="0" addon="false"/></land>)"
	R"(</house_lands>)";

constexpr const char* HOUSING_OBJECTS = R"(<housing_objects>)"
	R"(<passive area="INTERIOR" location="FLOOR" use_days="1" id="3000001" name_id="360001" category="CARPET" quality="COMMON" talking_distance="5.0" can_dye="true"/>)" // :3
	R"(<chair area="INTERIOR" location="FLOOR" use_days="1" id="3000004" name_id="360004" category="CHAIR" quality="COMMON" talking_distance="5.0" can_dye="true"/>)" // :6
	R"(<storage warehouse_id="1" area="INTERIOR" location="FLOOR" limit="STORAGE" id="3000007" name_id="360007" category="TABLE" quality="COMMON" talking_distance="5.0" can_dye="true"/>)" // :9
	R"(<postbox area="INTERIOR" location="FLOOR" id="3000016" name_id="360016" category="DECORATION" quality="COMMON" talking_distance="5.0"/>)" // :18
	R"(<npc npc_id="810013" area="INTERIOR" location="FLOOR" use_days="30" id="3001000" name_id="461892" category="NPC" quality="COMMON" talking_distance="5.0"/>)" // :46
	R"(<emblem level="1" area="ALL" location="WALL" id="3020041" name_id="799057" category="TABLE" quality="RARE" talking_distance="5.0"/>)" // :131
	R"(<picture area="INTERIOR" location="WALL" limit="PICTURE" id="3050001" name_id="794792" category="DECORATION" quality="UNIQUE" talking_distance="5.0"/>)" // :296
	R"(<chair area="INTERIOR" location="FLOOR" id="3120000" name_id="784584" category="BED" quality="COMMON" talking_distance="5.0" can_dye="true"/>)" // :440
	R"(<use_item use_count="20" delay="3000" cd="10" owner="false" area="INTERIOR" location="STACK" limit="COOKING" use_days="30" id="3190034" name_id="796750" category="DECORATION" quality="UNIQUE" talking_distance="2.0"><action reward_id="160010196" final_reward_id="188051654"/></use_item>)" // :898-900
	R"(<moviejukebox area="INTERIOR" location="WALL" limit="JUKEBOX" id="3190070" name_id="799831" category="DECORATION" quality="UNIQUE" talking_distance="2.0"/>)" // :997
	R"(<jukebox area="INTERIOR" location="WALL" limit="JUKEBOX" id="3999001" name_id="1" category="DECORATION" quality="COMMON" talking_distance="2.0"/>)" // synthetic
	R"(<move_item area="INTERIOR" location="FLOOR" id="3999002" name_id="1" category="DECORATION" quality="COMMON" talking_distance="2.0"/>)" // synthetic
	R"(</housing_objects>)";

constexpr const char* ITEMS = R"(<item_templates>)"
	R"(<item_template id="170190034" name="[Event] Solorius Cake" level="1" cName="Item_VisitorTree_BirthCake_01" mask="20552" quality="UNIQUE" price="100" desc="796667" activate_target="STANDALONE" activate_count="1000">)"
	R"(<actions><houseobject id="3190034"/></actions><acquisition type="REWARD" item="186000177" count="30"/><uselimits usedelayid="91"/></item_template>)"
	R"(<item_template id="170120000" name="Ruko Fiber Bed" level="1" cName="Item_bed_Combine_TypeA_c_10_01" mask="20606" quality="COMMON" price="200" desc="784584" activate_target="STANDALONE" activate_count="1000">)"
	R"(<actions><houseobject id="3120000"/></actions><uselimits usedelayid="91"/></item_template>)"
	R"(<item_template id="170000023" name="Octagonal Board Roof" level="1" cName="item_housing_add_cp_a002_roof_01" mask="20606" quality="COMMON" price="5" desc="790490" activate_target="STANDALONE" activate_count="1000">)"
	R"(<actions><housedeco/></actions><uselimits usedelayid="91"/></item_template>)"
	R"(</item_templates>)";

/** house_parts.xml:149, 150, 160 (the studio building's default parts) and :179 (the wallpaper) */
constexpr const char* HOUSE_PARTS = R"(<house_parts>)"
	R"(<house_part id="3533000" name="Fancy Wooden Door" quality="LEGEND" type="DOOR" building_tags="CP_D"/>)"
	R"(<house_part id="3534000" name="Plain Wallpaper" quality="LEGEND" type="INWALL_ANY" building_tags="CP_S CP_A CP_B CP_C CP_D"/>)"
	R"(<house_part id="3535000" name="Rough Wooden Floor" quality="LEGEND" type="INFLOOR_ANY" building_tags="CP_S CP_A CP_B CP_C CP_D"/>)"
	R"(<house_part id="3554000" name="Superb Plain Wallpaper" quality="UNIQUE" type="INWALL_ANY" building_tags="CP_S CP_A CP_B CP_C CP_D"/>)"
	R"(</house_parts>)";

/** house_buildings.xml:99-105, the studio building */
constexpr const char* HOUSE_BUILDINGS = R"(<buildings><building id="355000" type="PERSONAL_INS" size="STUDIO" parts_match="CP_D">)"
	R"(<parts><door>3533000</door><inwall>3534000</inwall><infloor>3535000</infloor></parts></building></buildings>)";

#define STUDIO_REQUIRE_DATABASE()                                                                                                                     \
	if (!lh::isDatabaseEnabled())                                                                                                                     \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

class StudioHouseTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		if (lh::isDatabaseEnabled()) {
			lh::setUpDatabaseOnce();
			lh::execute("DELETE FROM player_registered_items");
			lh::execute("DELETE FROM houses");
		}
		items::publishPoetaWorldDataOnce();
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(context, STUDIO_LANDS));
		dataholders::DataManager::HOUSING_OBJECT_DATA.publish(xml::bindString<dataholders::HousingObjectData>(context, HOUSING_OBJECTS));
		dataholders::DataManager::HOUSE_PARTS_DATA.publish(xml::bindString<dataholders::HousePartsData>(context, HOUSE_PARTS));
		dataholders::DataManager::HOUSE_BUILDING_DATA.publish(xml::bindString<dataholders::HouseBuildingData>(context, HOUSE_BUILDINGS));
		itemData = xml::bindString<dataholders::ItemData>(context, ITEMS);
	}

	void TearDown() override {
		for (PlayerFixture& owner : owners)
			if (owner.player) {
				world::World::getInstance().removeObject(*owner.player);
				owner.player->setClientConnection(nullptr);
			}
		clients.clear();
		owners.clear();
		dataholders::DataManager::HOUSE_BUILDING_DATA.resetForTests();
		dataholders::DataManager::HOUSE_PARTS_DATA.resetForTests();
		dataholders::DataManager::HOUSING_OBJECT_DATA.resetForTests();
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	/** an online player on a recording connection, with a players row (the houses' and the registered items' owner) */
	go::player::Player& owner(int32_t objectId, std::string_view name, model::Race race) {
		PlayerFixture& f = owners.emplace_back(makePlayer(objectId, objectId, name, race));
		f.player->getPosition()->setIsSpawned(true);
		// in the World, as an online player is: HousingService.notifyAboutOwnerChange finds him and resets his house list
		world::World::getInstance().storeObject(*f.player);
		clients.push_back(std::make_unique<TestClient>());
		clients.back()->enterWorld(f);
		(*clients.back())->clearSent();
		if (lh::isDatabaseEnabled())
			lh::insertPlayer(objectId, name, objectId, race == model::Race::ELYOS ? "ELYOS" : "ASMODIANS");
		return *f.player;
	}

	int32_t count(size_t client, AionServerPacket&& packet) {
		const std::vector<uint8_t> expected = serialized(std::move(packet), clients[client]->con());
		int32_t n = 0;
		for (const SerializedBody& body : (*clients[client])->sent())
			n += *body.bytes == expected ? 1 : 0;
		return n;
	}

	const model::templates::item::ItemTemplate* item(int32_t id) const { return itemData->getItemTemplate(id); }

	xml::LoadContext context;
	std::unique_ptr<dataholders::ItemData> itemData;
	std::vector<PlayerFixture> owners;
	std::vector<std::unique_ptr<TestClient>> clients;
};

TEST_F(StudioHouseTest, EachRaceGetsItsOwnStudioAndASecondOneIsRefused) {
	STUDIO_REQUIRE_DATABASE();
	go::player::Player& elyos = owner(410001, "Elyosowner", model::Race::ELYOS);
	go::player::Player& asmodian = owner(410002, "Asmoowner", model::Race::ASMODIANS);
	services::HousingService& housing = services::HousingService::getInstance();

	housing.registerPlayerStudio(elyos);
	housing.registerPlayerStudio(asmodian);

	runtime::Ptr<House> elyosStudio = housing.getPlayerStudio(elyos.getObjectId());
	runtime::Ptr<House> asmodianStudio = housing.getPlayerStudio(asmodian.getObjectId());
	ASSERT_TRUE(elyosStudio);
	ASSERT_TRUE(asmodianStudio);
	EXPECT_EQ(elyosStudio->getAddress()->getId(), 2001) << "HouseData.getStudioAddress(ELYOS)";
	EXPECT_EQ(asmodianStudio->getAddress()->getId(), 3001) << "HouseData.getStudioAddress(ASMODIANS)";
	EXPECT_EQ(asmodianStudio->getAddress()->getMapId(), 730010000);
	EXPECT_EQ(asmodianStudio->getAddress()->getExitMapId(), 710010000);
	EXPECT_EQ(asmodianStudio->getOwnerId(), asmodian.getObjectId());
	EXPECT_EQ(count(1, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_INS_OWN_SUCCESS()), 1);
	EXPECT_EQ(*lh::queryLong("SELECT COUNT(*) FROM houses WHERE address = 3001 AND building_id = 355000 AND player_id = " +
	                         std::to_string(asmodian.getObjectId())),
		1)
	  << "changeOwner's house.save()";

	housing.registerPlayerStudio(asmodian);
	EXPECT_EQ(count(1, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_INS_CANT_OWN_MORE_HOUSE()), 1) << "createStudio: the player's houses are not empty";
	EXPECT_EQ(*lh::queryLong("SELECT COUNT(*) FROM houses"), 2);
}

TEST_F(StudioHouseTest, TheFactoryBuildsTheObjectOfEveryTemplateKind) {
	runtime::Ref<House> house = go::VisibleObject::create<House>(dataholders::DataManager::HOUSE_DATA->getAddress(2001), 0);
	runtime::Ref<HouseRegistry> registry = HouseRegistry::create(*house);
	int32_t objectId = 7000;
	const auto build = [&](int32_t templateId) { return HouseObjectFactory::createNew(*registry, ++objectId, templateId); };
	// HouseObjectFactory.java:42-66: the instanceof chain, in its order
	EXPECT_TRUE(dynamic_cast<go::ChairObject*>(build(3000004).get())) << "chair";
	EXPECT_TRUE(dynamic_cast<go::ChairObject*>(build(3120000).get())) << "a bed is a chair template";
	EXPECT_TRUE(dynamic_cast<go::JukeBoxObject*>(build(3999001).get())) << "jukebox";
	EXPECT_TRUE(dynamic_cast<go::JukeBoxObject*>(build(3190070).get())) << "moviejukebox: HousingMovieJukeBox extends HousingJukeBox";
	EXPECT_TRUE(dynamic_cast<go::MoveableObject*>(build(3999002).get())) << "move_item";
	EXPECT_TRUE(dynamic_cast<go::NpcObject*>(build(3001000).get())) << "npc";
	EXPECT_TRUE(dynamic_cast<go::PictureObject*>(build(3050001).get())) << "picture";
	EXPECT_TRUE(dynamic_cast<go::PostboxObject*>(build(3000016).get())) << "postbox";
	EXPECT_TRUE(dynamic_cast<go::StorageObject*>(build(3000007).get())) << "storage";
	EXPECT_TRUE(dynamic_cast<go::UseableItemObject*>(build(3190034).get())) << "use_item";
	EXPECT_TRUE(dynamic_cast<go::EmblemObject*>(build(3020041).get())) << "emblem";
	runtime::Ref<go::HouseObject> passive = build(3000001);
	EXPECT_TRUE(dynamic_cast<go::PassiveObject*>(passive.get())) << "passive: the fallback";
	EXPECT_FALSE(dynamic_cast<go::ChairObject*>(passive.get()));
	EXPECT_EQ(passive->getObjectId(), objectId);
	EXPECT_THROW(build(3999999), runtime::NullPointerException) << "no template";
}

TEST_F(StudioHouseTest, AnItemsHouseObjectExpiresAfterItsUseDays) {
	runtime::Ref<House> house = go::VisibleObject::create<House>(dataholders::DataManager::HOUSE_DATA->getAddress(2001), 0);
	const int64_t now = commons::utils::currentTimeMillis() / 1000;
	runtime::Ref<go::HouseObject> cake = HouseObjectFactory::createNew(*house, item(170190034));
	EXPECT_TRUE(dynamic_cast<go::UseableItemObject*>(cake.get()));
	EXPECT_EQ(cake->getObjectTemplate()->getTemplateId(), 3190034);
	EXPECT_NEAR(static_cast<double>(cake->getExpireTime()), static_cast<double>(now + 30 * 86400), 5.0)
	  << "HouseObjectFactory.java:80-84: now + use_days";
	runtime::Ref<go::HouseObject> bed = HouseObjectFactory::createNew(*house, item(170120000));
	EXPECT_EQ(bed->getExpireTime(), 0) << "no use_days: no expiry";
	EXPECT_NE(bed->getObjectId(), cake->getObjectId()) << "IDFactory.nextId";
	EXPECT_THROW(HouseObjectFactory::createNew(*house, item(170000023)), runtime::NullPointerException)
	  << "a decoration item has no SummonHouseObjectAction (Objects.requireNonNull)";
}

TEST_F(StudioHouseTest, ABareHousedecoHasTemplateIdZero) {
	const model::templates::item::ItemTemplate* roof = item(170000023);
	ASSERT_NE(roof, nullptr);
	ASSERT_NE(roof->getActions(), nullptr);
	const model::templates::item::actions::DecorateAction* decorate = roof->getActions()->getDecorateAction();
	ASSERT_NE(decorate, nullptr) << "<housedeco/>";
	EXPECT_EQ(decorate->getTemplateId(), 0) << "DecorateAction.java:28-32: no id attribute -> 0";
	EXPECT_EQ(item(170190034)->getActions()->getDecorateAction(), nullptr);
}

TEST_F(StudioHouseTest, TheRegistryStoredIsTheRegistryLoaded) {
	STUDIO_REQUIRE_DATABASE();
	go::player::Player& player = owner(410003, "Collector", model::Race::ELYOS);
	services::HousingService::getInstance().registerPlayerStudio(player);
	runtime::Ptr<House> studio = services::HousingService::getInstance().getPlayerStudio(player.getObjectId());
	ASSERT_TRUE(studio);
	runtime::Ptr<HouseRegistry> registry = studio->getRegistry();
	ASSERT_TRUE(registry);

	runtime::Ref<go::HouseObject> cake = HouseObjectFactory::createNew(*studio, item(170190034));
	cake->setX(366.5f);
	cake->setY(295.25f);
	cake->setZ(222.35f);
	cake->setRotation(90);
	registry->putObject(*cake, false);
	runtime::Ref<go::HouseObject> bed = HouseObjectFactory::createNew(*studio, item(170120000));
	bed->setX(363.0f);
	bed->setY(297.5f);
	bed->setZ(222.35f);
	registry->putObject(*bed, false);
	runtime::Ref<go::HouseDecoration> wallpaper = go::HouseDecoration::create(utils::idfactory::IDFactory::getInstance().nextId(), 3554000);
	registry->putDecor(*wallpaper, false);
	registry->setUsed(*wallpaper, 0);
	registry->save();
	EXPECT_EQ(*lh::queryLong("SELECT COUNT(*) FROM player_registered_items WHERE player_id = " + std::to_string(player.getObjectId())), 3);

	// the restart path: a fresh registry loads the rows and builds each object through createNew (PlayerRegisteredItemsDAO.constructObject)
	studio->reloadHouseRegistry();
	runtime::Ptr<HouseRegistry> loaded = studio->getRegistry();
	ASSERT_TRUE(loaded);
	ASSERT_NE(loaded.get(), registry.get());
	runtime::Ptr<go::HouseObject> loadedCake = loaded->getObjectByObjId(cake->getObjectId());
	ASSERT_TRUE(loadedCake);
	EXPECT_TRUE(dynamic_cast<go::UseableItemObject*>(loadedCake.get()));
	EXPECT_FLOAT_EQ(loadedCake->getX(), 366.5f);
	EXPECT_FLOAT_EQ(loadedCake->getY(), 295.25f);
	EXPECT_EQ(loadedCake->getRotation(), 90);
	EXPECT_EQ(loadedCake->getExpireTime(), cake->getExpireTime());
	runtime::Ptr<go::HouseObject> loadedBed = loaded->getObjectByObjId(bed->getObjectId());
	ASSERT_TRUE(loadedBed);
	EXPECT_TRUE(dynamic_cast<go::ChairObject*>(loadedBed.get())) << "the bed comes back as a chair";
	EXPECT_EQ(loaded->getUsedDecorId(model::templates::housing::PartType::INWALL_ANY, 0), 3554000) << "the wallpaper in its room";
}

TEST_F(StudioHouseTest, AReusableStudioIsSavedAndReleasedOnDespawn) {
	STUDIO_REQUIRE_DATABASE();
	go::player::Player& player = owner(410004, "Leaver", model::Race::ELYOS);
	services::HousingService::getInstance().registerPlayerStudio(player);
	runtime::Ptr<House> studio = services::HousingService::getInstance().getPlayerStudio(player.getObjectId());
	ASSERT_TRUE(studio);
	studio->setSignNotice("back soon");
	studio->setPosition(world::WorldPosition::create(720010000, 366.242615f, 295.776093f, 222.3536f, int8_t{0}));

	studio->getController().onDespawn(); // HouseController.java:61-69, the studio instance's destroy

	EXPECT_FALSE(studio->getPosition()) << "the destroyed instance is released";
	EXPECT_FALSE(studio->getButler()) << "clearSpawns";
	EXPECT_EQ(*lh::queryLong("SELECT COUNT(*) FROM houses WHERE sign_notice = 'back soon' AND player_id = " + std::to_string(player.getObjectId())), 1)
	  << "the studio is saved: it stays in memory for the owner's next entry";
	EXPECT_TRUE(services::HousingService::getInstance().getPlayerStudio(player.getObjectId())) << "still the owner's studio";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
