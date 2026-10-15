// P4-07b housing, mail, item group, reward, item set, global drop and goods list templates: hooks on small XML fixtures and the logic methods,
// with expected values derived by hand from the Java sources of these packages (Building.java, HouseAddress.java, HousingLand.java,
// PlaceableHouseObject.java and its subclasses, the housing enums, Mails.java, SysMail.java, MailTemplate.java, MailPart.java,
// ItemRaceEntry.java and the rewards classes, BonusItemGroup.java, ItemSetTemplate.java, GlobalDropItem.java, GoodsList.java).

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseBuildingData.bind.h"
#include "aion/gameserver/dataholders/HouseBuildingData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/Chance.h"
#include "aion/gameserver/model/limiteditems/LimitedItem.h"
#include "aion/gameserver/model/templates/QuestTemplate.bind.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.bind.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalRule.bind.h"
#include "aion/gameserver/model/templates/goods/GoodsList.bind.h"
#include "aion/gameserver/model/templates/housing/Building.bind.h"
#include "aion/gameserver/model/templates/housing/BuildingTypeInfo.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.bind.h"
#include "aion/gameserver/model/templates/housing/HousePart.bind.h"
#include "aion/gameserver/model/templates/housing/HouseTypeInfo.h"
#include "aion/gameserver/model/templates/housing/HousingCategoryInfo.h"
#include "aion/gameserver/model/templates/housing/HousingChair.bind.h"
#include "aion/gameserver/model/templates/housing/HousingEmblem.bind.h"
#include "aion/gameserver/model/templates/housing/HousingLand.bind.h"
#include "aion/gameserver/model/templates/housing/HousingMovieJukeBox.bind.h"
#include "aion/gameserver/model/templates/housing/LimitTypeInfo.h"
#include "aion/gameserver/model/templates/housing/PartTypeInfo.h"
#include "aion/gameserver/model/templates/housing/PlaceAreaInfo.h"
#include "aion/gameserver/model/templates/housing/PlaceLocationInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/itemgroups/CraftItemGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/CraftRecipeGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/EnchantGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/EventGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/FeedEntries.h"
#include "aion/gameserver/model/templates/itemgroups/FoodGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/GatherGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/ManastoneGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/MedalGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/MedicineGroup.bind.h"
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.bind.h"
#include "aion/gameserver/model/templates/mail/IMailFormatter.h"
#include "aion/gameserver/model/templates/mail/MailMessageInfo.h"
#include "aion/gameserver/model/templates/mail/MailPartTypeInfo.h"
#include "aion/gameserver/model/templates/mail/Mails.bind.h"
#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"
#include "aion/gameserver/model/templates/rewards/RewardEntryItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::model::templates {
namespace {

template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

std::string failureOf(const std::function<void()>& action) {
	try {
		action();
	} catch (const xml::StaticDataException& e) {
		return e.what();
	}
	return "<no failure>";
}

/** Publishes a holder into DataManager for one test and forgets it again, also when an assertion ends the test early */
template <class H>
class PublishedHolder {
public:
	PublishedHolder(xml::HolderRef<H>& holderRef, std::unique_ptr<H> holder) : ref(holderRef) { ref.publish(std::move(holder)); }
	~PublishedHolder() { ref.resetForTests(); }
	PublishedHolder(const PublishedHolder&) = delete;
	PublishedHolder& operator=(const PublishedHolder&) = delete;

private:
	xml::HolderRef<H>& ref;
};

// ---- housing -----------------------------------------------------------------------------------------------------------------------------------

constexpr std::string_view LAND_XML = R"(<land id="1" sign_nosale="1" sign_sale="2" sign_waiting="3" sign_home="4" manager_npc="5" teleport_npc="6">)"
                                      R"(<addresses><address id="10" x="1" y="2" z="3" town="0" map="700010000"/>)"
                                      R"(<address id="11" x="1" y="2" z="3" town="0" map="700010000" exit_map="1"/></addresses>)"
                                      R"(<buildings><building id="21"/><building id="22" default="true"/></buildings>)"
                                      R"(<sale point_price="1" gold_price="2" level="3"/><fee>500</fee>)"
                                      R"(<caps addon="false" emblemId="0" floor="true" room="true" interior="1" exterior="2"/></land>)";

TEST(HousingTemplatesTest, BuildingPartsAndLands) {
	std::unique_ptr<housing::Building> building =
	  bindXml<housing::Building>(R"(<building id="1" parts_match="CP_A" size="ESTATE" type="PERSONAL_FIELD">)"
	                             R"(<parts><roof>2</roof><outwall>3</outwall><door>7</door><infloor>8</infloor></parts></building>)");
	EXPECT_EQ(building->getPartsMatchTag(), "CP_A");
	EXPECT_EQ(building->getSize(), housing::HouseType::ESTATE);
	EXPECT_EQ(building->getType(), housing::BuildingType::PERSONAL_FIELD);
	EXPECT_EQ(building->getDefaultDecorId(housing::PartType::ROOF), 2);
	EXPECT_EQ(building->getDefaultDecorId(housing::PartType::DOOR), 7);
	EXPECT_EQ(building->getDefaultDecorId(housing::PartType::FENCE), std::nullopt) << "no fence element";
	EXPECT_EQ(building->getDefaultDecorId(housing::PartType::INWALL_ANY), std::nullopt) << "inwall 0 is not a part";
	EXPECT_EQ(building->getDefaultPartIds(), (std::vector<int32_t>{2, 3, 7, 8})) << "EnumMap order: ROOF, OUTWALL, DOOR, INFLOOR_ANY";

	std::unique_ptr<housing::HousePart> part =
	  bindXml<housing::HousePart>(R"(<house_part id="1" quality="COMMON" type="ROOF" building_tags="CP_A CP_B"/>)");
	EXPECT_TRUE(part->isForBuilding(*building));
	std::unique_ptr<housing::Building> other = bindXml<housing::Building>(R"(<building id="2" parts_match="CP_C"><parts/></building>)");
	EXPECT_FALSE(part->isForBuilding(*other));
	EXPECT_TRUE(other->getDefaultPartIds().empty());

	std::unique_ptr<housing::HousingLand> land = bindXml<housing::HousingLand>(LAND_XML);
	ASSERT_TRUE(land->getAddresses().has_value());
	for (const housing::HouseAddress& address : *land->getAddresses())
		EXPECT_EQ(address.getLand(), land.get()) << "the hook stores the parent land";
	EXPECT_EQ(land->getDefaultBuilding()->getId(), 22);
	EXPECT_EQ(land->hashCode(), 1);
	std::string withoutDefault(LAND_XML);
	withoutDefault.replace(withoutDefault.find(R"( default="true")"), 15, "");
	EXPECT_EQ(bindXml<housing::HousingLand>(withoutDefault)->getDefaultBuilding()->getId(), 21) << "the first building";
	EXPECT_NE(failureOf([] {
		          bindXml<housing::HouseAddress>(R"(<address id="10" x="1" y="2" z="3" town="0" map="1"/>)");
	          }).find("HouseAddress 10 is not bound inside a HousingLand"),
	          std::string::npos);
}

TEST(HousingTemplatesTest, LandBuildingsReadTheHolderBuilding) {
	// Java Building.getPartsMatchTag/getSize/getType/getPartsByType: a building of a land template has only id and default, the other values come
	// from DataManager.HOUSE_BUILDING_DATA.getBuilding(id)
	std::unique_ptr<housing::HousingLand> land = bindXml<housing::HousingLand>(LAND_XML);
	ASSERT_TRUE(land->getBuildings().has_value());
	const housing::Building& landBuilding = land->getBuildings()->at(0);
	ASSERT_EQ(landBuilding.getId(), 21);
	EXPECT_THROW(static_cast<void>(landBuilding.getPartsMatchTag()), runtime::NullPointerException) << "HOUSE_BUILDING_DATA is not published";
	PublishedHolder<dataholders::HouseBuildingData> published(
	  dataholders::DataManager::HOUSE_BUILDING_DATA,
	  bindXml<dataholders::HouseBuildingData>(R"(<buildings><building id="21" parts_match="CP_F" size="HOUSE" type="PERSONAL_INS">)"
	                                          R"(<parts><roof>5</roof><fence>4</fence></parts></building>)"
	                                          R"(<building id="22" parts_match="CP_G" size="STUDIO" type="PERSONAL_FIELD"/></buildings>)"));
	EXPECT_EQ(landBuilding.getPartsMatchTag(), "CP_F");
	EXPECT_EQ(landBuilding.getSize(), housing::HouseType::HOUSE);
	EXPECT_EQ(landBuilding.getType(), housing::BuildingType::PERSONAL_INS);
	EXPECT_EQ(landBuilding.getDefaultDecorId(housing::PartType::FENCE), 4);
	EXPECT_EQ(landBuilding.getDefaultPartIds(), (std::vector<int32_t>{5, 4})) << "EnumMap order: ROOF before FENCE, whatever the XML order";
	const housing::Building& defaultBuilding = land->getBuildings()->at(1);
	EXPECT_EQ(defaultBuilding.getSize(), housing::HouseType::STUDIO);
	EXPECT_THROW(static_cast<void>(defaultBuilding.getDefaultPartIds()), runtime::NullPointerException)
	  << "Java: the holder's building has partsByType == null, then values() on null";

	std::unique_ptr<housing::Building> own = bindXml<housing::Building>(R"(<building id="21" size="ESTATE"/>)");
	EXPECT_EQ(own->getSize(), housing::HouseType::ESTATE) << "an own value wins";
	EXPECT_EQ(own->getPartsMatchTag(), "CP_F") << "parts_match is null: the holder's";

	std::unique_ptr<housing::Building> unknown = bindXml<housing::Building>(R"(<building id="99"/>)");
	EXPECT_THROW(static_cast<void>(unknown->getType()), runtime::NullPointerException) << "Java: getBuilding(99) is null";
}

TEST(HousingTemplatesTest, PlaceableObjectsAndEnumCompanions) {
	std::unique_ptr<housing::HousingChair> chair =
	  bindXml<housing::HousingChair>(R"(<chair id="1" talking_distance="2" quality="COMMON" category="CHAIR" name_id="3" use_days="7" limit="POT"/>)");
	const housing::PlaceableHouseObject& placeable = *chair;
	EXPECT_EQ(placeable.getTypeId(), 5) << "selected by javaClassName";
	EXPECT_EQ(chair->getTypeId(), 5);
	EXPECT_EQ(placeable.getUseDays(), 7);
	EXPECT_EQ(placeable.getPlacementLimit(), housing::LimitType::POT);
	EXPECT_EQ(placeable.getName(), "") << "Java null";
	std::unique_ptr<housing::HousingEmblem> emblem =
	  bindXml<housing::HousingEmblem>(R"(<emblem id="1" talking_distance="2" quality="COMMON" category="DECORATION" name_id="3" level="1"/>)");
	EXPECT_EQ(static_cast<const housing::PlaceableHouseObject&>(*emblem).getTypeId(), 11);
	EXPECT_EQ(emblem->getUseDays(), 0);
	EXPECT_EQ(emblem->getPlacementLimit(), housing::LimitType::NONE);
	std::unique_ptr<housing::HousingMovieJukeBox> movie =
	  bindXml<housing::HousingMovieJukeBox>(R"(<moviejukebox id="1" talking_distance="2" quality="COMMON" category="DECORATION" name_id="3"/>)");
	EXPECT_EQ(static_cast<const housing::PlaceableHouseObject&>(*movie).getTypeId(), 0) << "HousingMovieJukeBox overrides HousingJukeBox's 6";

	EXPECT_EQ(housing::getId(housing::BuildingType::PERSONAL_FIELD), 2);
	EXPECT_EQ(housing::getId(housing::HouseType::STUDIO), 0);
	EXPECT_EQ(housing::getLimitTypeIndex(housing::HouseType::PALACE), 4);
	EXPECT_EQ(housing::getAbbreviation(housing::HouseType::MANSION), "b");
	EXPECT_EQ(housing::getObjectPlaceLimit(housing::LimitType::VISITOR_POT, housing::HouseType::HOUSE), 2);
	EXPECT_EQ(housing::getTrialObjectPlaceLimit(housing::LimitType::POT, housing::HouseType::STUDIO), 1);
	EXPECT_EQ(housing::getId(housing::LimitType::JUKEBOX), 7);
	EXPECT_EQ(housing::getRooms(housing::PartType::INFLOOR_ANY), 6);
	EXPECT_EQ(housing::getForLineNr(10), housing::PartType::INWALL_ANY);
	EXPECT_EQ(housing::getForLineNr(7), std::nullopt) << "7 is unused";
	EXPECT_EQ(housing::getForLineNr(27), housing::PartType::ADDON);
	EXPECT_EQ(housing::fromValue<housing::PlaceArea>("INTERIOR"), housing::PlaceArea::INTERIOR);
	EXPECT_EQ(housing::fromValue<housing::PlaceLocation>("WALL"), housing::PlaceLocation::WALL);
	EXPECT_EQ(housing::fromValue<housing::HousingCategory>("NPC"), housing::HousingCategory::NPC);
	EXPECT_EQ(housing::fromValue<housing::LimitType>("COOKING"), housing::LimitType::COOKING);
	EXPECT_EQ(housing::value(housing::PlaceArea::ALL), "ALL");
	EXPECT_THROW(housing::fromValue<housing::PlaceArea>("all"), commons::utils::IllegalArgumentException);
}

// ---- mail --------------------------------------------------------------------------------------------------------------------------------------

struct CashItemFormatter final : mail::IMailFormatter {
	mail::MailPartType getType() const override { return mail::MailPartType::CUSTOM; }
	std::string getFormattedString(mail::MailPartType) const override { return ""; }
	std::string getParamValue(std::string_view name) const override { return name == "itemid" ? "187000001" : name == "count" ? "3" : ""; }
};

TEST(MailTemplatesTest, SystemMailsFormatting) {
	std::unique_ptr<mail::Mails> mails =
	  bindXml<mail::Mails>(R"(<mails>)"
	                       R"(<mail name="$$CASH_ITEM_MAIL"><template name="Shop" race="PC_ALL">)"
	                       R"(<sender id="10"/><title id="0"><param id="itemid"/><param id="count"/></title>)"
	                       R"(<header id="20"/><body type="HEADER" id="30"><param id="count"/></body><tail id="0"/></template>)"
	                       R"(<template name="Race" race="ELYOS"><title id="1"/><header id="0"/><body id="0"/><tail id="0"/></template>)"
	                       R"(<template name="race" race="ASMODIANS"><title id="2"/><header id="0"/><body id="0"/><tail id="0"/></template></mail>)"
	                       R"(<mail name="$$cash_item_mail"><template name="Other" race="PC_ALL"><title id="5"/></template></mail>)"
	                       R"(<mail name="$$HS_AUCTION"><template name="x" race="PC_ALL"><title/></template></mail>)"
	                       R"(</mails>)");
	EXPECT_EQ(mails->size(), 2) << "the names are lower-cased keys; the second $$CASH_ITEM_MAIL replaces the first";
	EXPECT_EQ(mails->getMailTemplate("$$CASH_ITEM_MAIL", "Shop", Race::ELYOS), nullptr) << "replaced by the later mail";
	const mail::MailTemplate* other = mails->getMailTemplate("$$Cash_Item_Mail", "other", Race::ELYOS);
	ASSERT_NE(other, nullptr);
	EXPECT_EQ(other->getFormattedTitle(nullptr), "5");
	EXPECT_EQ(mails->getMailTemplate("unknown", "", Race::PC_ALL), nullptr);

	std::unique_ptr<mail::Mails> single =
	  bindXml<mail::Mails>(R"(<mails>)"
	                       R"(<mail name="$$CASH_ITEM_MAIL"><template name="Shop" race="PC_ALL">)"
	                       R"(<sender id="10"/><title id="0"><param id="itemid"/><param id="count"/></title>)"
	                       R"(<header id="20"/><body type="HEADER" id="30"><param id="count"/></body><tail id="0"/></template>)"
	                       R"(<template name="Race" race="ELYOS"><title id="1"/><header id="0"/><body id="0"/><tail id="0"/></template>)"
	                       R"(<template name="race" race="ASMODIANS"><title id="2"/><header id="0"/><body id="0"/><tail id="0"/></template></mail>)"
	                       R"(<mail name="$$HS_AUCTION"><template name="x" race="PC_ALL"><title/></template></mail></mails>)");
	const mail::MailTemplate* shop = single->getMailTemplate("$$cash_item_mail", "SHOP", Race::ASMODIANS);
	ASSERT_NE(shop, nullptr) << "PC_ALL matches every race";
	ASSERT_NE(shop->getSender(), nullptr);
	EXPECT_EQ(shop->getSender()->getType(), mail::MailPartType::SENDER);
	EXPECT_EQ(shop->getTitle()->getType(), mail::MailPartType::TITLE);
	EXPECT_EQ(shop->getBody(), nullptr) << "the body part declares type HEADER and replaces the header part in the map";
	EXPECT_EQ(shop->getHeader()->getFormattedString(nullptr), "30") << "an empty parameter value is not appended";
	CashItemFormatter formatter;
	EXPECT_EQ(shop->getFormattedTitle(&formatter), "187000001,3") << "id 0 adds nothing";
	EXPECT_EQ(shop->getFormattedTitle(nullptr), ",") << "the parts' own getParamValue returns empty values";
	EXPECT_THROW(shop->getFormattedMessage(&formatter), runtime::NullPointerException) << "no body part";

	EXPECT_EQ(single->getMailTemplate("$$CASH_ITEM_MAIL", "race", Race::ELYOS)->getTitle()->getFormattedString(nullptr), "1");
	EXPECT_EQ(single->getMailTemplate("$$CASH_ITEM_MAIL", "race", Race::ASMODIANS)->getTitle()->getFormattedString(nullptr), "2")
	  << "Race and race share the lower-case event: the first template of the race";
	const mail::MailTemplate* race = single->getMailTemplate("$$CASH_ITEM_MAIL", "Race", Race::ELYOS);
	EXPECT_EQ(race->getFormattedMessage(nullptr), "");
	EXPECT_THROW(single->getMailTemplate("$$HS_AUCTION", "X", Race::PC_ALL)->getFormattedTitle(nullptr), runtime::NullPointerException)
	  << "a part without id";

	EXPECT_EQ(mail::getId(mail::MailMessage::MAILSPAM_WAIT_FOR_SOME_TIME), 6);
	EXPECT_EQ(mail::fromValue("TAIL"), mail::MailPartType::TAIL);
	EXPECT_EQ(mail::value(mail::MailPartType::CUSTOM), "CUSTOM");
}

// ---- item groups, rewards, item sets ---------------------------------------------------------------------------------------------------------

TEST(ItemGroupTemplatesTest, EntryClassesAndGroups) {
	xml::LoadContext context;
	// the templates stay alive: the context keeps their XmlIDs
	std::unique_ptr<item::ItemTemplate> common = xml::bindString<item::ItemTemplate>(context, R"(<item_template id="186000001" race="PC_ALL"/>)");
	std::unique_ptr<item::ItemTemplate> elyos = xml::bindString<item::ItemTemplate>(context, R"(<item_template id="186000002" race="ELYOS"/>)");
	commons::utils::Rnd::seedCurrentThreadForTests(7);

	std::unique_ptr<itemgroups::CraftItemGroup> craft = xml::bindString<itemgroups::CraftItemGroup>(
	  context,
	  R"(<craft_materials bonusType="MATERIAL" chance="30"><item id="186000001" skill="40001" minLevel="0" maxLevel="99"/></craft_materials>)");
	EXPECT_FLOAT_EQ(craft->getChance(), 30.0f);
	EXPECT_EQ(craft->getGroupClass(), itemgroups::BonusItemGroup::GroupClass::CRAFT_ITEM);
	std::vector<const itemgroups::ItemRaceEntry*> craftItems = static_cast<const itemgroups::BonusItemGroup&>(*craft).getItems();
	ASSERT_EQ(craftItems.size(), 1u);
	EXPECT_EQ(craftItems[0], &craft->getItems()[0]);
	EXPECT_EQ(craftItems[0]->getEntryClass(), itemgroups::ItemRaceEntry::EntryClass::CRAFT_ITEM);
	for (int i = 0; i < 20; ++i) {
		int64_t count = craftItems[0]->getCount();
		EXPECT_GE(count, 3);
		EXPECT_LE(count, 5);
	}
	EXPECT_FLOAT_EQ(craftItems[0]->getChance(), 100.0f);

	std::unique_ptr<itemgroups::EventGroup> events = xml::bindString<itemgroups::EventGroup>(
	  context, R"(<events bonusType="EVENTS"><item id="186000002" race="ELYOS" level="0" count="42" chance="12.5"/></events>)");
	std::vector<const itemgroups::ItemRaceEntry*> eventItems = static_cast<const itemgroups::BonusItemGroup&>(*events).getItems();
	ASSERT_EQ(eventItems.size(), 1u);
	EXPECT_EQ(eventItems[0]->getCount(), 42) << "FullRewardItem through ItemRaceEntry";
	EXPECT_FLOAT_EQ(eventItems[0]->getChance(), 12.5f);
	EXPECT_EQ(model::Chance::selectElement(eventItems), eventItems[0]);

	std::unique_ptr<itemgroups::FoodGroup> food =
	  xml::bindString<itemgroups::FoodGroup>(context, R"(<food bonusType="FOOD"><item id="186000001" level="0"/></food>)");
	int64_t foodCount = static_cast<const itemgroups::BonusItemGroup&>(*food).getItems()[0]->getCount();
	EXPECT_TRUE(foodCount == 5 || foodCount == 10) << foodCount;
	std::unique_ptr<itemgroups::ManastoneGroup> manastones =
	  xml::bindString<itemgroups::ManastoneGroup>(context, R"(<manastones_common bonusType="MANASTONE"><item id="186000001"/></manastones_common>)");
	EXPECT_EQ(static_cast<const itemgroups::BonusItemGroup&>(*manastones).getItems()[0]->getCount(), 1);

	EXPECT_NE(failureOf([&] {
		          xml::bindString<itemgroups::ManastoneGroup>(context, R"(<m bonusType="MANASTONE"><item id="1"/></m>)");
	          }).find("BonusItemGroup item ID 1 is invalid"),
	          std::string::npos);
	EXPECT_NE(failureOf([&] {
		          xml::bindString<itemgroups::ManastoneGroup>(context, R"(<m bonusType="MANASTONE"><item id="186000002" race="ASMODIANS"/></m>)");
	          }).find("BonusItemGroup item 186000002 has invalid race ASMODIANS. Item is only for ELYOS"),
	          std::string::npos);

	itemgroups::FeedEntries::FeedFluid fluid;
	EXPECT_EQ(fluid.getRace(), Race::PC_ALL);
}

/** Calls the protected matchesQuest/matchesLevel of ItemRaceEntry through a base reference (Java's overridable methods), as matches() does */
struct EntryDispatch : itemgroups::ItemRaceEntry {
	static bool quest(const itemgroups::ItemRaceEntry& entry, const QuestTemplate& questTemplate) {
		return (entry.*(&EntryDispatch::matchesQuest))(questTemplate);
	}
	static bool level(const itemgroups::ItemRaceEntry& entry, const item::ItemTemplate& itemTemplate, int32_t bonusItemLevel) {
		return (entry.*(&EntryDispatch::matchesLevel))(itemTemplate, bonusItemLevel);
	}
};

TEST(ItemGroupTemplatesTest, MatchesDispatchOnTheEntryClass) {
	xml::LoadContext context;
	// the item data is published below and leaked by resetForTests, so the XmlIDs of the context stay valid
	std::unique_ptr<dataholders::ItemData> itemData = xml::bindString<dataholders::ItemData>(
	  context, R"(<item_templates><item_template id="186000010" level="30"/><item_template id="186000011" level="0" race="ELYOS"/></item_templates>)");
	const item::ItemTemplate* level30 = itemData->getItemTemplate(186000010);
	ASSERT_NE(level30, nullptr);
	auto quest = [](const std::string& rest) { return bindXml<QuestTemplate>(R"(<quest id="1" )" + rest + "</quest>"); };
	auto entryOf = [](const itemgroups::BonusItemGroup& group, size_t index) -> const itemgroups::ItemRaceEntry& {
		return *group.getItems().at(index);
	};

	using EntryClass = itemgroups::ItemRaceEntry::EntryClass;
	std::unique_ptr<itemgroups::GatherGroup> gather =
	  xml::bindString<itemgroups::GatherGroup>(context, R"(<gather bonusType="GATHER"><item id="186000010"/></gather>)");
	std::unique_ptr<itemgroups::EnchantGroup> enchant =
	  xml::bindString<itemgroups::EnchantGroup>(context, R"(<enchant bonusType="ENCHANT"><item id="186000010" level="10"/></enchant>)");
	std::unique_ptr<itemgroups::FoodGroup> food =
	  xml::bindString<itemgroups::FoodGroup>(context, R"(<food bonusType="FOOD"><item id="186000011" level="10"/></food>)");
	std::unique_ptr<itemgroups::MedicineGroup> medicine =
	  xml::bindString<itemgroups::MedicineGroup>(context, R"(<medicine bonusType="MEDICINE"><item id="186000010" level="10"/></medicine>)");
	std::unique_ptr<itemgroups::MedalGroup> medals =
	  xml::bindString<itemgroups::MedalGroup>(context, R"(<medals bonusType="MEDAL"><item id="186000010" level="10" count="2" chance="50"/></medals>)");
	std::unique_ptr<itemgroups::CraftItemGroup> craftItems = xml::bindString<itemgroups::CraftItemGroup>(
	  context, R"(<craft bonusType="MATERIAL"><item id="186000010" skill="40001" minLevel="100" maxLevel="199"/></craft>)");
	std::unique_ptr<itemgroups::CraftRecipeGroup> recipes = xml::bindString<itemgroups::CraftRecipeGroup>(
	  context,
	  R"(<recipes bonusType="RECIPE"><item id="186000010" skill="40001" level="120"/><item id="186000010" skill="40002" level="480"/></recipes>)");
	const itemgroups::ItemRaceEntry& plain = entryOf(*gather, 0);
	const itemgroups::ItemRaceEntry& idLevel = entryOf(*enchant, 0);
	const itemgroups::ItemRaceEntry& foodItem = entryOf(*food, 0);
	const itemgroups::ItemRaceEntry& medicineItem = entryOf(*medicine, 0);
	const itemgroups::ItemRaceEntry& fullReward = entryOf(*medals, 0);
	const itemgroups::ItemRaceEntry& craftItem = entryOf(*craftItems, 0);
	const itemgroups::ItemRaceEntry& recipe120 = entryOf(*recipes, 0);
	const itemgroups::ItemRaceEntry& recipe480 = entryOf(*recipes, 1);
	EXPECT_EQ(plain.getEntryClass(), EntryClass::ITEM_RACE_ENTRY);
	EXPECT_EQ(idLevel.getEntryClass(), EntryClass::ID_LEVEL_REWARD);
	EXPECT_EQ(foodItem.getEntryClass(), EntryClass::FOOD_ITEM);
	EXPECT_EQ(medicineItem.getEntryClass(), EntryClass::MEDICINE_ITEM);
	EXPECT_EQ(fullReward.getEntryClass(), EntryClass::FULL_REWARD_ITEM);
	EXPECT_EQ(craftItem.getEntryClass(), EntryClass::CRAFT_ITEM);
	EXPECT_EQ(recipe120.getEntryClass(), EntryClass::CRAFT_RECIPE);

	// matchesLevel: ItemRaceEntry compares the bonus level with the item level, IdLevelReward (and FoodItem, MedicineItem, FullRewardItem) with
	// its own level; CraftReward does not override it
	for (const itemgroups::ItemRaceEntry* itemLevelEntry : {&plain, &craftItem, &recipe120}) {
		EXPECT_TRUE(EntryDispatch::level(*itemLevelEntry, *level30, 0));
		EXPECT_TRUE(EntryDispatch::level(*itemLevelEntry, *level30, 30));
		EXPECT_FALSE(EntryDispatch::level(*itemLevelEntry, *level30, 10)) << static_cast<int>(itemLevelEntry->getEntryClass());
	}
	for (const itemgroups::ItemRaceEntry* ownLevelEntry : {&idLevel, &foodItem, &medicineItem, &fullReward}) {
		EXPECT_TRUE(EntryDispatch::level(*ownLevelEntry, *level30, 0));
		EXPECT_TRUE(EntryDispatch::level(*ownLevelEntry, *level30, 10));
		EXPECT_FALSE(EntryDispatch::level(*ownLevelEntry, *level30, 30)) << static_cast<int>(ownLevelEntry->getEntryClass());
	}

	// matchesQuest: true except for the craft rewards (skill, then CraftItem's min/max level or CraftRecipe's getMaxLevel)
	std::unique_ptr<QuestTemplate> anyQuest = quest(">");
	for (const itemgroups::ItemRaceEntry* entry : {&plain, &idLevel, &foodItem, &medicineItem, &fullReward})
		EXPECT_TRUE(EntryDispatch::quest(*entry, *anyQuest)) << static_cast<int>(entry->getEntryClass());
	EXPECT_FALSE(EntryDispatch::quest(craftItem, *anyQuest)) << "CraftReward: combineskill 0 != 40001";
	auto craftQuest = [&](int32_t skill, int32_t point) {
		return quest("combineskill=\"" + std::to_string(skill) + "\" combine_skillpoint=\"" + std::to_string(point) + "\">");
	};
	EXPECT_TRUE(EntryDispatch::quest(craftItem, *craftQuest(40001, 100)));
	EXPECT_TRUE(EntryDispatch::quest(craftItem, *craftQuest(40001, 199)));
	EXPECT_FALSE(EntryDispatch::quest(craftItem, *craftQuest(40001, 99)));
	EXPECT_FALSE(EntryDispatch::quest(craftItem, *craftQuest(40001, 200)));
	EXPECT_FALSE(EntryDispatch::quest(craftItem, *craftQuest(40002, 150))) << "another skill";
	// CraftRecipe.getMaxLevel() = Math.min(level + 40, level / 100 * 100 + 99): level 120 -> min(160, 199) = 160, level 480 -> min(520, 499) = 499
	EXPECT_FALSE(EntryDispatch::quest(recipe120, *craftQuest(40001, 119)));
	EXPECT_TRUE(EntryDispatch::quest(recipe120, *craftQuest(40001, 120)));
	EXPECT_TRUE(EntryDispatch::quest(recipe120, *craftQuest(40001, 160)));
	EXPECT_FALSE(EntryDispatch::quest(recipe120, *craftQuest(40001, 161)));
	EXPECT_FALSE(EntryDispatch::quest(recipe120, *craftQuest(40002, 150))) << "another skill";
	EXPECT_TRUE(EntryDispatch::quest(recipe480, *craftQuest(40002, 499)));
	EXPECT_FALSE(EntryDispatch::quest(recipe480, *craftQuest(40002, 500)));

	// matches: race, then level, then quest, with the item template of DataManager.ITEM_DATA
	std::unique_ptr<QuestTemplate> bonus10 = quest(R"(><bonus type="FOOD" level="10"/>)");
	EXPECT_THROW(static_cast<void>(foodItem.matches(Race::ELYOS, *bonus10)), runtime::NullPointerException) << "ITEM_DATA is not published";
	PublishedHolder<dataholders::ItemData> published(dataholders::DataManager::ITEM_DATA, std::move(itemData));
	EXPECT_TRUE(foodItem.matches(Race::ELYOS, *bonus10));
	EXPECT_FALSE(foodItem.matches(Race::ASMODIANS, *bonus10)) << "the item is ELYOS only";
	EXPECT_FALSE(foodItem.matches(Race::ELYOS, *quest(R"(><bonus type="FOOD" level="20"/>)")));
	EXPECT_FALSE(plain.matches(Race::ASMODIANS, *bonus10)) << "item level 30";
	EXPECT_TRUE(plain.matches(Race::ASMODIANS, *quest(R"(><bonus type="FOOD" level="30"/>)")));
	EXPECT_TRUE(recipe120.matches(Race::ELYOS, *quest(R"(combineskill="40001" combine_skillpoint="130"><bonus type="RECIPE"/>)")));
	EXPECT_FALSE(recipe120.matches(Race::ELYOS, *quest(R"(combineskill="40001" combine_skillpoint="170"><bonus type="RECIPE"/>)")));
	EXPECT_THROW(static_cast<void>(recipe120.matches(Race::ELYOS, *anyQuest)), runtime::NullPointerException) << "Java: getBonus().getLevel()";
}

TEST(ItemGroupTemplatesTest, RewardItemsAndItemSets) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<rewards::RewardEntryItem> entry = rewards::RewardEntryItem::create(9, 186000001, 5);
	EXPECT_EQ(entry->getEntryId(), 9);
	EXPECT_EQ(entry->getId(), 186000001);
	EXPECT_EQ(entry->getCount(), 5);
	EXPECT_EQ(entry->toString(), "RewardItem [id=186000001, count=5]");
	runtime::Ref<rewards::ArenaRewardItem> arena = rewards::ArenaRewardItem::create(1, 2, 3, 4);
	EXPECT_EQ(arena->getTotalCount(), 9);
	EXPECT_EQ(arena->hashCode(), 31810) << "((1 * 31 + 2) * 31 + 3) * 31 + 4";
	EXPECT_TRUE(arena->equals(*rewards::ArenaRewardItem::create(1, 2, 3, 4)));
	EXPECT_FALSE(arena->equals(*rewards::ArenaRewardItem::create(1, 2, 3, 5)));
	EXPECT_EQ(arena->toString(), "ArenaRewardItem[itemId=1, baseCount=2, rankingCount=3, scoreCount=4]");

	std::unique_ptr<itemset::ItemSetTemplate> set = bindXml<itemset::ItemSetTemplate>(
	  R"(<itemset id="1" name="Set">)"
	  R"(<itempart itemid="1"/><itempart itemid="2"/><itempart itemid="3"/><partbonus count="2"/><fullbonus/></itemset>)");
	ASSERT_NE(set->getFullbonus(), nullptr);
	EXPECT_EQ(set->getFullbonus()->getCount(), 3) << "the hook sets the number of items";
	EXPECT_EQ(set->getFullbonus()->getModifiers(), nullptr);
	EXPECT_EQ(set->getPartbonus().at(0).getModifiers(), nullptr);
	EXPECT_EQ(bindXml<itemset::ItemSetTemplate>(R"(<itemset id="2"><itempart itemid="1"/><partbonus count="1"/></itemset>)")->getFullbonus(), nullptr);
}

// ---- global drops, goods lists ---------------------------------------------------------------------------------------------------------------

TEST(DropTemplatesTest, GlobalDropItemHookAndLists) {
	xml::LoadContext context;
	std::unique_ptr<item::ItemTemplate> item =
	  xml::bindString<item::ItemTemplate>(context, R"(<item_template id="182400001"/>)"); // kept alive for its XmlID
	std::unique_ptr<globaldrops::GlobalRule> rule = xml::bindString<globaldrops::GlobalRule>(
	  context, R"(<gd_rule rule_name="r" chance="1">)"
	           R"(<gd_items><gd_item id="182400001" min_count="2"/><gd_item id="182400001" max_count="5" chance="3.5"/></gd_items>)"
	           R"(<gd_npc_names><gd_npc_name value="a" function="CONTAINS"/></gd_npc_names></gd_rule>)");
	ASSERT_TRUE(rule->getDropItems().has_value());
	const std::vector<globaldrops::GlobalDropItem>& items = *rule->getDropItems();
	EXPECT_EQ(items[0].getMaxCount(), 2) << "max_count defaults to min_count";
	EXPECT_FLOAT_EQ(items[0].getChance(), 100.0f);
	EXPECT_EQ(items[1].getMinCount(), 1);
	EXPECT_EQ(items[1].getMaxCount(), 5);
	EXPECT_FLOAT_EQ(items[1].getChance(), 3.5f);
	EXPECT_EQ(rule->getGlobalRuleNpcNames()->getGlobalDropNpcNames().size(), 1u);

	EXPECT_NE(
	  failureOf([&] { xml::bindString<globaldrops::GlobalDropItem>(context, R"(<gd_item id="1"/>)"); }).find("Global drop item ID 1 is invalid"),
	  std::string::npos);
	EXPECT_NE(failureOf([&] {
		          xml::bindString<globaldrops::GlobalDropItem>(context, R"(<gd_item id="182400001" min_count="0"/>)");
	          }).find("Global drop item [182400001] min_count (0) must be greater than 0"),
	          std::string::npos);
	EXPECT_NE(failureOf([&] {
		          xml::bindString<globaldrops::GlobalDropItem>(context, R"(<gd_item id="182400001" min_count="3" max_count="2"/>)");
	          }).find("Global drop item [182400001] max_count (2) must be greater than or equal to min_count (3)"),
	          std::string::npos);
}

TEST(DropTemplatesTest, GoodsListHookAndLimitedItems) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	std::unique_ptr<goods::GoodsList> goods =
	  bindXml<goods::GoodsList>(R"(<list id="7" legion_lvl="2"><salestime>0 0 0 ? * MON</salestime>)"
	                            R"(<item id="10" sell_limit="5" buy_limit="6"/><item id="11"/><item id="12" sell_limit="1"/></list>)");
	EXPECT_EQ(goods->getItemIdList(), (std::vector<int32_t>{10, 11, 12}));
	std::vector<runtime::Ref<limiteditems::LimitedItem>> limited = goods->getLimitedItems();
	ASSERT_EQ(limited.size(), 1u) << "only items with both limits";
	EXPECT_EQ(limited[0]->getItemId(), 10);
	EXPECT_EQ(limited[0]->getSellLimit(), 5);
	EXPECT_EQ(limited[0]->getBuyLimit(), 6);
	EXPECT_EQ(limited[0]->getSalesTime(), "0 0 0 ? * MON");
	EXPECT_NE(goods->getLimitedItems()[0].get(), limited[0].get()) << "a new list of new objects per call";
	EXPECT_TRUE(bindXml<goods::GoodsList>(R"(<list id="8"/>)")->getItemIdList().empty());
}

} // namespace
} // namespace aion::gameserver::model::templates
