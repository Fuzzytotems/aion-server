// M5b-3 leftovers CP3 (P5-07): ArmsfusionService, ItemPurificationService, ItemRemodelService, WarehouseService and UpgradeArcadeService
// against their Java (services/ArmsfusionService.java, services/item/ItemPurificationService.java, services/item/ItemRemodelService.java,
// services/WarehouseService.java, services/UpgradeArcadeService.java). HouseObjectFactory is tested in tests/legionhouse/HouseModelTest.cpp
// (it needs a house), expandWarehouse's question in tests/playersvc/DialogServiceTest.cpp. The player is ItemServicesTest's "Looter"
// (700101, ELYOS, level 1, abyss rank 1); the prices are Java's defaults (prices.properties: 100 each).

#include "ItemServicesTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/dataholders/ItemPurificationData.bind.h"
#include "aion/gameserver/dataholders/ItemPurificationData.h"
#include "aion/gameserver/dataholders/UpgradeArcadeData.bind.h"
#include "aion/gameserver/dataholders/UpgradeArcadeData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPGRADE_ARCADE.h"
#include "aion/gameserver/services/ArmsfusionService.h"
#include "aion/gameserver/services/UpgradeArcadeService.h"
#include "aion/gameserver/services/WarehouseService.h"
#include "aion/gameserver/services/item/ItemPurificationService.h"
#include "aion/gameserver/services/item/ItemRemodelService.h"
#include "aion/gameserver/services/trade/PricesService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using namespace std::chrono_literals;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::aion::serverpackets::SM_UPGRADE_ARCADE;

constexpr int32_t ARCADE_TOKEN = 186000389; // :897124
constexpr int32_t ITEM = 850301;            // the first item's object id

constexpr std::string_view CP3_ROWS = R"xml(
	<item_template id="186000389" name="Arcade Token" level="1" cName="world_cash_ingame_gacha_coin" mask="12360" max_stack_count="10000" quality="UNIQUE" price="5" desc="839099">
		<inventory id="0"/>
	</item_template>
)xml";

/** Synthetic (the shipped rows need item templates the fixture lacks): Tahabata's Sword purifies to the Training Sword for two potions and 100 AP */
constexpr std::string_view PURIFICATIONS_XML = R"xml(<item_purifications>
	<item_purification base_item_id="100000768">
		<purification_result result_item_id="100000094" min_enchant_count="5" necessary_abyss_points="100" necessary_kinah="1000">
			<req_material item_id="162000002" item_count="2" />
		</purification_result>
	</item_purification>
</item_purifications>)xml";

/** Synthetic arcadelist.xml (the chances made certain, the reward a fixture item): level 1 always succeeds, the maximum is 3 */
constexpr std::string_view ARCADE_XML = R"xml(<arcadelist>
	<levels min_resumable_level="2">
		<level level="1" icon="success_weapon01" upgrade_chance="100" />
		<level level="2" icon="success_weapon01" upgrade_chance="0" />
		<level level="3" icon="success_weapon02" />
	</levels>
	<rewards min_level="1">
		<item item_id="162000002" normal_count="3" frenzy_count="6" />
	</rewards>
</arcadelist>)xml";

std::string withRows(std::string_view base, std::string_view closingTag, std::string_view rows) {
	std::string xml(base);
	xml.insert(xml.rfind(closingTag), rows);
	return xml;
}

class ItemServicesCp3Test : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, withRows(ITEM_TEMPLATES_XML, "</item_templates>", CP3_ROWS)));
		dataholders::DataManager::ITEM_PURIFICATION_DATA.publish(xml::bindString<dataholders::ItemPurificationData>(context, PURIFICATIONS_XML));
		dataholders::DataManager::UPGRADE_ARCADE_DATA.publish(xml::bindString<dataholders::UpgradeArcadeData>(context, ARCADE_XML));
		player().setSkillList(model::skill::PlayerSkillList::create()); // the AP changes ask the abyss rank skills
	}

	void TearDown() override {
		ItemServicesTest::TearDown();
		dataholders::DataManager::UPGRADE_ARCADE_DATA.resetForTests();
		dataholders::DataManager::ITEM_PURIFICATION_DATA.resetForTests();
	}

	int64_t count(const std::vector<std::vector<uint8_t>>& packets, const std::vector<uint8_t>& packet) {
		return std::count(packets.begin(), packets.end(), packet);
	}

	AtomicConfigScope<int32_t> prices{configs::main::PricesConfig::DEFAULT_PRICES, 100};
	AtomicConfigScope<int32_t> modifier{configs::main::PricesConfig::DEFAULT_MODIFIER, 100};
	AtomicConfigScope<int32_t> taxes{configs::main::PricesConfig::DEFAULT_TAXES, 100};
	xml::LoadContext context;
};

// ---- ArmsfusionService (ArmsfusionService.java:22-138) ----------------------------------------------------------------------------------------

TEST_F(ItemServicesCp3Test, FusionRefusesMissingItemsMoneyAndAHigherSecondWeapon) {
	clearSent();
	ArmsfusionService::fusionWeapons(player(), ITEM, ITEM + 1);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_COMPOUND_ITEM_NO_TARGET_ITEM())}));

	Item& tahabata = stored(ITEM, TAHABATA_SWORD, 1);
	Item& training = stored(ITEM + 1, TRAINING_SWORD, 1);
	clearSent();
	ArmsfusionService::fusionWeapons(player(), ITEM, ITEM + 1);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_NOT_ENOUGH_MONEY(tahabata.getL10n(), training.getL10n()))}))
		<< "EPIC: 500 * 50 * 50 kinah";

	stored(ITEM + 2, KINAH, trade::PricesService::getPriceForService(200, player().getRace()));
	clearSent();
	ArmsfusionService::fusionWeapons(player(), ITEM + 1, ITEM);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_MAIN_REQUIRE_HIGHER_LEVEL())})) << "COMMON: 200 * 1 * 1 paid";
}

TEST_F(ItemServicesCp3Test, FusionJoinsTheSecondWeaponAndBreakingSeparatesIt) {
	Item& tahabata = stored(ITEM, TAHABATA_SWORD, 1);
	Item& training = stored(ITEM + 1, TRAINING_SWORD, 1);
	const int64_t price = trade::PricesService::getPriceForService(500 * 50 * 50, player().getRace()); // EPIC, level 50
	Item& kinah = stored(ITEM + 2, KINAH, price + 7);
	clearSent();
	ArmsfusionService::fusionWeapons(player(), ITEM, ITEM + 1);
	EXPECT_TRUE(tahabata.hasFusionedItem());
	EXPECT_EQ(tahabata.getFusionedItemId(), TRAINING_SWORD);
	EXPECT_FALSE(player().getInventory().getItemByObjId(ITEM + 1)) << "the second weapon is used up";
	EXPECT_EQ(kinah.getItemCount(), 7);
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_COMPOUND_SUCCESS(tahabata.getL10n(), training.getL10n()))), 1);

	clearSent();
	ArmsfusionService::breakWeapons(player(), ITEM);
	EXPECT_FALSE(tahabata.hasFusionedItem());
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_COMPOUNDED_ITEM_DECOMPOUND_SUCCESS(tahabata.getL10n()))), 1);
	clearSent();
	ArmsfusionService::breakWeapons(player(), ITEM);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_DECOMPOUND_ERROR_NOT_AVAILABLE(tahabata.getL10n()))}));
	clearSent();
	ArmsfusionService::breakWeapons(player(), ITEM + 9);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_DECOMPOUND_ITEM_NO_TARGET_ITEM())}));
}

// ---- ItemPurificationService (ItemPurificationService.java:29-140) -------------------------------------------------------------------------------

TEST_F(ItemServicesCp3Test, PurificationChecksEnchantApKinahAndMaterialsInJavasOrder) {
	Item& sword = stored(ITEM, TAHABATA_SWORD, 1);
	EXPECT_FALSE(ItemPurificationService::isPurificationAllowed(player(), sword, TAHABATA_SWORD)) << "no such result";
	clearSent();
	EXPECT_FALSE(ItemPurificationService::isPurificationAllowed(player(), sword, TRAINING_SWORD));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_REGISTER_ITEM_MSG_UPGRADE_CANNOT(sword.getL10n()))})) << "enchant 0 < 5";
	sword.setEnchantLevel(7);
	clearSent();
	EXPECT_FALSE(ItemPurificationService::isPurificationAllowed(player(), sword, TRAINING_SWORD));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_REGISTER_ITEM_MSG_UPGRADE_CANNOT_NEED_AP())}));
	player().getAbyssRank()->addAp(150);
	clearSent();
	EXPECT_FALSE(ItemPurificationService::isPurificationAllowed(player(), sword, TRAINING_SWORD));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_REGISTER_ITEM_MSG_UPGRADE_CANNOT_NEED_QINA())}));
	stored(ITEM + 1, KINAH, 1000);
	clearSent();
	EXPECT_FALSE(ItemPurificationService::isPurificationAllowed(player(), sword, TRAINING_SWORD)) << "the potions are missing";
	EXPECT_TRUE(sent().empty());
	stored(ITEM + 2, MINOR_LIFE_POTION, 3);
	clearSent();
	EXPECT_TRUE(ItemPurificationService::isPurificationAllowed(player(), sword, TRAINING_SWORD));
	const model::templates::item::ItemTemplate* training = dataholders::DataManager::ITEM_DATA->getItemTemplate(TRAINING_SWORD);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_ITEM_UPGRADE_MSG_UPGRADE_SUCCESS(sword.getL10n(), training->getL10n()))}));
}

// java-bug kept (proposed correction): decreaseKinah(-necessaryKinah) takes nothing
TEST_F(ItemServicesCp3Test, PurificationTakesTheMaterialsAndApButNotTheKinah) {
	Item& sword = stored(ITEM, TAHABATA_SWORD, 1);
	Item& kinah = stored(ITEM + 1, KINAH, 1000);
	Item& potions = stored(ITEM + 2, MINOR_LIFE_POTION, 3);
	player().getAbyssRank()->addAp(150);
	const int32_t apBefore = player().getAbyssRank()->getAp();
	EXPECT_TRUE(ItemPurificationService::decreaseMaterials(player(), sword, TRAINING_SWORD));
	EXPECT_EQ(potions.getItemCount(), 1);
	EXPECT_EQ(apBefore - player().getAbyssRank()->getAp(), 100);
	EXPECT_EQ(kinah.getItemCount(), 1000) << "Java's decreaseKinah of a negative amount";
	EXPECT_FALSE(player().getInventory().getItemByObjId(ITEM)) << "the base item is used up";
}

TEST_F(ItemServicesCp3Test, ThePurifiedItemKeepsTheSourcesEnchantMinusFive) {
	Item& sword = stored(ITEM, TAHABATA_SWORD, 1);
	sword.setEnchantLevel(9);
	sword.setSoulBound(true);
	sword.setItemColor(0x123456);
	ItemPurificationService::upgradeItem(player(), sword, TRAINING_SWORD);
	std::vector<Ptr<Item>> trainings = player().getInventory().getItemsByItemId(TRAINING_SWORD);
	ASSERT_EQ(trainings.size(), 1u);
	EXPECT_EQ(trainings[0]->getEnchantLevel(), 4);
	EXPECT_TRUE(trainings[0]->isSoulBound());
	EXPECT_EQ(trainings[0]->getItemColor(), 0x123456);
}

// ---- ItemRemodelService (ItemRemodelService.java:20-123) --------------------------------------------------------------------------------------

TEST_F(ItemServicesCp3Test, RemodellingNeedsLevelTenAndKinahThenTakesTheSkin) {
	Item& keep = stored(ITEM, TRAINING_SWORD, 1);
	Item& extract = stored(ITEM + 1, TAHABATA_SWORD, 1);
	clearSent();
	ItemRemodelService::remodelItem(player(), ITEM, ITEM + 1);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_PC_LEVEL_LIMIT())}));
	player().getCommonData()->setLevel(11);
	ASSERT_GE(player().getLevel(), 10);
	clearSent();
	ItemRemodelService::remodelItem(player(), ITEM, ITEM + 1);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_NOT_ENOUGH_GOLD(keep.getItemTemplate()->getL10n()))}));
	Item& kinah = stored(ITEM + 2, KINAH, trade::PricesService::getPriceForService(1000, player().getRace()) + 500);
	extract.setItemColor(0x00ff00);
	clearSent();
	ItemRemodelService::remodelItem(player(), ITEM, ITEM + 1);
	EXPECT_EQ(keep.getItemSkinTemplate()->getTemplateId(), TAHABATA_SWORD);
	EXPECT_EQ(keep.getItemColor(), 0x00ff00) << "the dye moves along";
	EXPECT_EQ(kinah.getItemCount(), 500);
	EXPECT_FALSE(player().getInventory().getItemByObjId(ITEM + 1));
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_SUCCEED(keep.getItemTemplate()->getL10n()))), 1);
}

// ---- WarehouseService (WarehouseService.java:72-117) ---------------------------------------------------------------------------------------------

TEST_F(ItemServicesCp3Test, TheWarehouseExpandsToElevenAndNoFurther) {
	EXPECT_TRUE(WarehouseService::canExpand(player()));
	clearSent();
	WarehouseService::expand(player(), true);
	EXPECT_EQ(player().getWhNpcExpands(), 1);
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_SIZE_EXTENDED(8))), 1);
	EXPECT_TRUE(WarehouseService::canExpandByTicket(player(), 1)) << "no bonus expansion yet";
	player().getCommonData()->setWhBonusExpands(10);
	clearSent();
	EXPECT_FALSE(WarehouseService::canExpand(player())) << "1 + 10 expansions: the 12th is above MAX_EXPAND 11";
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_CANT_EXTEND_MORE())}));
	clearSent();
	WarehouseService::expand(player(), false);
	EXPECT_EQ(player().getWhBonusExpands(), 10);
}

// ---- UpgradeArcadeService (UpgradeArcadeService.java:36-181); one case: the service caches the progress per player for the process ----------

TEST_F(ItemServicesCp3Test, AnArcadeRunCostsATokenClimbsALevelAndPaysTheReward) {
	UpgradeArcadeService& arcade = UpgradeArcadeService::getInstance();
	clearSent();
	arcade.open(player());
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_UPGRADE_ARCADE())}));
	clearSent();
	arcade.getReward(player());
	arcade.resume(player());
	arcade.startTry(player());
	EXPECT_TRUE(sent().empty()) << "level 0: no reward, nothing to resume and no token to start with";
	EXPECT_EQ(arcade.getRewardsForLevel(5)->getMinLevel(), 1);
	EXPECT_EQ(arcade.getRewardsForLevel(0), nullptr);

	Item& tokens = stored(ITEM, ARCADE_TOKEN, 2);
	arcade.startTry(player());
	EXPECT_EQ(tokens.getItemCount(), 1);
	executor->advance(3000ms);
	clearSent();
	arcade.getReward(player());
	std::vector<Ptr<Item>> potions = player().getInventory().getItemsByItemId(MINOR_LIFE_POTION);
	ASSERT_EQ(potions.size(), 1u);
	EXPECT_EQ(potions[0]->getItemCount(), 3) << "the normal count: no frenzy after 8 points";
	EXPECT_EQ(count(sent(), serialized(SM_UPGRADE_ARCADE(MINOR_LIFE_POTION, int64_t{3}))), 1);
	clearSent();
	arcade.getReward(player());
	EXPECT_TRUE(sent().empty()) << "the reward resets the run to level 0";
}

} // namespace
} // namespace aion::gameserver::services::item::test
