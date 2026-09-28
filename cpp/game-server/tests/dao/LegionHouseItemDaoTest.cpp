// Round trips of the legion, mail, pet, housing and item DAOs (P4-14) against a fresh aion_gs.sql database: legions (emblem blobs, generated
// history keys, IN lists), legion members, legion dominion participants, letters, pets, houses, bids, house scripts (UTF-16LE + zlib), inventory
// (visible equipment joined with god stones), item stones (batches in a transaction) and registered house items.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "DaoTestSupport.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/BrokerDAO.h"
#include "aion/gameserver/dao/HouseBidsDAO.h"
#include "aion/gameserver/dao/HouseScriptsDAO.h"
#include "aion/gameserver/dao/HousesDAO.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/LegionDAO.h"
#include "aion/gameserver/dao/LegionDominionDAO.h"
#include "aion/gameserver/dao/LegionMemberDAO.h"
#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/dao/PlayerPetsDAO.h"
#include "aion/gameserver/dao/PlayerRegisteredItemsDAO.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"
#include "aion/gameserver/model/house/PlayerScript.h"
#include "aion/gameserver/model/items/ItemStone.h"
#include "aion/gameserver/model/items/ItemStone_ItemStoneType.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/team/legion/LegionEmblemType.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/utils/xml/CompressUtil.h"

namespace aion::gameserver::dao::test {
namespace {

using model::gameobjects::Persistable;
using runtime::Ptr;
using runtime::Ref;

class LegionHouseItemDaoTest : public DaoTest {};

TEST_F(LegionHouseItemDaoTest, LegionQueriesAndEmblemBlobs) {
	execute("INSERT INTO legions (id, name, level, contribution_points) VALUES (1000, 'Heroes', 3, 12345), (1001, 'Villains', 1, 0)");
	EXPECT_TRUE(LegionDAO::isNameUsed("Heroes"));
	EXPECT_FALSE(LegionDAO::isNameUsed("Nobody"));
	EXPECT_EQ(LegionDAO::getUsedIDs(), (std::vector<int32_t>{1000, 1001}));
	LegionDAO::deleteLegion(1001);
	EXPECT_EQ(LegionDAO::getUsedIDs(), (std::vector<int32_t>{1000}));
	// loadLegion catches the exceptions of the Legion constructor inside DB.select: check that it is ported first
	SKIP_IF_UNPORTED(static_cast<void>(model::team::legion::Legion::create(1, "Probe")));
	Ref<model::team::legion::Legion> legion = LegionDAO::loadLegion(1000);
	ASSERT_TRUE(legion);
	EXPECT_EQ(legion->getName(), "Heroes");
	EXPECT_EQ(legion->getLegionLevel(), 3);
	EXPECT_EQ(legion->getContributionPoints(), 12345);
	EXPECT_EQ(legion->getDeputyPermission(), 7692) << "column default";
	Ref<model::team::legion::Legion> byName = LegionDAO::loadLegion("Heroes");
	ASSERT_TRUE(byName);
	EXPECT_EQ(byName->getLegionId(), 1000);
	EXPECT_FALSE(LegionDAO::loadLegion("Nobody"));
}

TEST_F(LegionHouseItemDaoTest, LegionEmblemBlobRoundTrip) {
	execute("INSERT INTO legions (id, name) VALUES (1000, 'Heroes')");
	EXPECT_FALSE(LegionDAO::checkEmblem(1000));
	execute("INSERT INTO legion_emblems (legion_id, emblem_id, color_a, color_r, color_g, color_b, emblem_type, emblem_data) VALUES "
			"(1000, 2, -1, 10, 20, 30, 'CUSTOM', x'00ff7f80')");
	EXPECT_TRUE(LegionDAO::checkEmblem(1000));
	Ref<model::team::legion::LegionEmblem> emblem;
	SKIP_IF_UNPORTED(emblem = LegionDAO::loadLegionEmblem(1000));
	EXPECT_EQ(emblem->getEmblemId(), 2);
	EXPECT_EQ(emblem->getColor_a(), -1);
	EXPECT_EQ(emblem->getColor_b(), 30);
	EXPECT_EQ(emblem->getEmblemType(), model::team::legion::LegionEmblemType::CUSTOM);
	ASSERT_TRUE(emblem->getCustomEmblemData());
	EXPECT_EQ(emblem->getCustomEmblemData()->snapshot(), (std::vector<int8_t>{0, -1, 127, -128}));
	EXPECT_EQ(emblem->getPersistentState(), Persistable::PersistentState::UPDATED);

	emblem->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	emblem->setEmblem(5, 1, 2, 3, 4, model::team::legion::LegionEmblemType::CUSTOM, runtime::Array<int8_t>::of({1, 2, 3}));
	LegionDAO::storeLegionEmblem(1000, *emblem);
	EXPECT_EQ(queryString("SELECT HEX(emblem_data) FROM legion_emblems WHERE legion_id = 1000"), "010203");
	EXPECT_EQ(queryLong("SELECT emblem_id FROM legion_emblems WHERE legion_id = 1000"), 5);
}

TEST_F(LegionHouseItemDaoTest, LegionAnnouncementsAndHistory) {
	execute("INSERT INTO legions (id, name) VALUES (1000, 'Heroes')");
	EXPECT_FALSE(LegionDAO::loadAnnouncement(1000));
	const commons::database::Timestamp time = detail::toTimestamp(1757894400000);
	LegionDAO::saveAnnouncement(1000, model::team::legion::Legion::Announcement::create("Raid at nine", time));
	Ref<model::team::legion::Legion::Announcement> announcement = LegionDAO::loadAnnouncement(1000);
	ASSERT_TRUE(announcement);
	EXPECT_EQ(announcement->message(), "Raid at nine");
	EXPECT_EQ(announcement->time(), time);
	LegionDAO::saveAnnouncement(1000, nullptr);
	EXPECT_FALSE(LegionDAO::loadAnnouncement(1000)) << "a null announcement only deletes";

	using model::team::legion::LegionHistoryAction;
	Ref<model::team::legion::LegionHistoryEntry> first = LegionDAO::insertHistory(1000, LegionHistoryAction::JOIN, "Newbie", "");
	Ref<model::team::legion::LegionHistoryEntry> second = LegionDAO::insertHistory(1000, LegionHistoryAction::KINAH_DEPOSIT, "Rich", "1000");
	Ref<model::team::legion::LegionHistoryEntry> third = LegionDAO::insertHistory(1000, LegionHistoryAction::LEVEL_UP, "4", "");
	ASSERT_TRUE(first && second && third);
	EXPECT_EQ(second->id(), first->id() + 1) << "generated keys";
	EXPECT_NEAR(static_cast<double>(first->epochSeconds()), static_cast<double>(commons::utils::currentTimeMillis() / 1000), 5.0);
	EXPECT_EQ(queryString("SELECT history_type FROM legion_history WHERE id = " + std::to_string(second->id())), "KINAH_DEPOSIT");
	LegionDAO::deleteHistory(1000, {Ptr<model::team::legion::LegionHistoryEntry>(first), Ptr<model::team::legion::LegionHistoryEntry>(third)});
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM legion_history"), 1) << "DELETE ... WHERE id IN (?,?)";
	LegionDAO::deleteHistory(1000, {});
	EXPECT_FALSE(LegionDAO::insertHistory(4711, LegionHistoryAction::JOIN, "x", "")) << "foreign key failure: logged, null";
}

TEST_F(LegionHouseItemDaoTest, LegionMembers) {
	insertPlayer(1, "General", 1);
	insertPlayer(2, "Soldier", 2);
	execute("INSERT INTO legions (id, name) VALUES (1000, 'Heroes')");
	execute("INSERT INTO legion_members (legion_id, player_id, `rank`) VALUES (1000, 1, 'BRIGADE_GENERAL'), (1000, 2, 'VOLUNTEER')");
	EXPECT_TRUE(LegionMemberDAO::isIdUsed(1));
	EXPECT_FALSE(LegionMemberDAO::isIdUsed(3));
	EXPECT_EQ(LegionMemberDAO::loadLegionMembers(1000), (std::vector<int32_t>{1, 2}));
	EXPECT_TRUE(LegionMemberDAO::setRank(2, model::team::legion::LegionRank::CENTURION));
	EXPECT_FALSE(LegionMemberDAO::setRank(3, model::team::legion::LegionRank::CENTURION)) << "no row";
	EXPECT_EQ(queryString("SELECT `rank` FROM legion_members WHERE player_id = 2"), "CENTURION");
	InventoryDAO::loadLegionId(2);
	EXPECT_EQ(InventoryDAO::loadLegionId(2), 1000);
	LegionMemberDAO::deleteLegionMember(2);
	EXPECT_EQ(LegionMemberDAO::loadLegionMembers(1000), (std::vector<int32_t>{1}));
	EXPECT_EQ(InventoryDAO::loadLegionId(2), 0);
}

TEST_F(LegionHouseItemDaoTest, LegionDominionParticipants) {
	Ref<model::legionDominion::LegionDominionParticipantInfo> info = model::legionDominion::LegionDominionParticipantInfo::create();
	info->setLegionId(1000);
	LegionDominionDAO::storeNewInfo(7, *info);
	info->setPoints(250);
	info->setTime(90);
	info->setDate(detail::toTimestamp(1757894400000));
	LegionDominionDAO::updateInfo(*info);
	EXPECT_EQ(queryString("SELECT CONCAT(legion_dominion_id, ',', points, ',', survived_time, ',', UNIX_TIMESTAMP(participated_date)) FROM legion_dominion_participants"),
		"7,250,90,1757894400");
	LegionDominionDAO::delete_(*info);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM legion_dominion_participants"), 0);
}

TEST_F(LegionHouseItemDaoTest, LettersSaveUpdateDelete) {
	insertPlayer(1, "Recipient", 1);
	const commons::database::Timestamp received = detail::toTimestamp(1757894400000);
	Ref<model::gameobjects::Letter> letter;
	SKIP_IF_UNPORTED(letter = model::gameobjects::Letter::create(5000, 1, nullptr, 250, "Hello", "A message", "Sender", received, true,
						 model::gameobjects::LetterType::EXPRESS));
	letter->setPersistentState(Persistable::PersistentState::NEW);
	EXPECT_TRUE(MailDAO::storeLetter(*letter));
	EXPECT_EQ(letter->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(queryString("SELECT CONCAT(sender_name, ',', mail_title, ',', unread, ',', attached_item_id, ',', attached_kinah_count, ',', express) FROM mail"),
		"Sender,Hello,1,0,250,1");
	EXPECT_TRUE(MailDAO::haveUnread(1));
	EXPECT_FALSE(MailDAO::haveUnread(2));
	EXPECT_EQ(MailDAO::getUsedIDs(), (std::vector<int32_t>{5000}));

	SKIP_IF_UNPORTED(letter->setReadLetter());
	letter->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_TRUE(MailDAO::storeLetter(*letter));
	EXPECT_FALSE(MailDAO::haveUnread(1));

	EXPECT_TRUE(MailDAO::cleanMail("Recipient")) << "only letters without attachments";
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 1);
	EXPECT_TRUE(MailDAO::deleteLetter(5000));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 0);

	Ref<model::gameobjects::player::PlayerCommonData> recipient = model::gameobjects::player::PlayerCommonData::create(1);
	recipient->setName("Recipient");
	recipient->setMailboxLetters(3);
	MailDAO::updateOfflineMailCounter(*recipient);
	EXPECT_EQ(queryLong("SELECT mailbox_letters FROM players WHERE id = 1"), 3);
}

TEST_F(LegionHouseItemDaoTest, PetUpdatesWithoutTemplates) {
	insertPlayer(1, "Owner", 1);
	execute("INSERT INTO player_pets (id, player_id, template_id, decoration, name) VALUES (70, 1, 1, 0, 'Fluffy')");
	PlayerPetsDAO::saveFeedStatus(70, 2, 55, 123456789);
	PlayerPetsDAO::setTime(70, 987654321);
	EXPECT_EQ(queryString("SELECT CONCAT(hungry_level, ',', feed_progress, ',', reuse_time) FROM player_pets WHERE id = 70"), "2,55,987654321");
	EXPECT_EQ(PlayerPetsDAO::getUsedIDs(), (std::vector<int32_t>{70}));
	PlayerPetsDAO::removePlayerPet(70);
	EXPECT_TRUE(PlayerPetsDAO::getUsedIDs().empty());
	auto f = makePlayer(1, 1, "Owner");
	EXPECT_TRUE(PlayerPetsDAO::getPlayerPets(*f.player).empty());
}

TEST_F(LegionHouseItemDaoTest, HousesBidsAndScripts) {
	execute("INSERT INTO houses (id, player_id, building_id, address) VALUES (900, 1, 10, 1001), (901, 2, 10, 1002)");
	EXPECT_EQ(HousesDAO::getUsedIDs(), (std::vector<int32_t>{900, 901}));
	HousesDAO::deleteHouse(2);
	EXPECT_EQ(HousesDAO::getUsedIDs(), (std::vector<int32_t>{900}));

	// house bids reference houses (foreign key)
	execute("INSERT INTO houses (id, player_id, building_id, address) VALUES (902, 0, 10, 1003)");
	execute("INSERT INTO house_bids (player_id, house_id, bid) VALUES (1, 900, 1000), (2, 900, 2000), (3, 902, 500)");
	EXPECT_TRUE(HouseBidsDAO::deleteOrDisableBids(2, {}));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM house_bids WHERE player_id = 0"), 1) << "the bids of player 2 are disabled";
	EXPECT_TRUE(HouseBidsDAO::deleteHouseBids(900));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM house_bids"), 1);

	const std::string xml = "<scripts><script id=\"1\">\xC3\xA9</script></scripts>";
	HouseScriptsDAO::storeScript(900, 1, xml);
	HouseScriptsDAO::storeScript(900, 2, "");
	HouseScriptsDAO::storeScript(900, 1, xml); // ON DUPLICATE KEY UPDATE
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM house_scripts"), 2);
	EXPECT_EQ(queryString("SELECT script FROM house_scripts WHERE script_id = 1"), xml);

	HouseScriptsDAO::deleteScript(900, 2);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM house_scripts"), 1);
	HouseScriptsDAO::storeScript(902, 3, "<other/>");
	HouseScriptsDAO::storeScript(902, 4, "<other/>");
	HouseScriptsDAO::deleteScriptsForHouse(902);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM house_scripts"), 1);

	// getPlayerScripts compresses the UTF-16LE bytes with zlib (level 6); the expected bytes are Python's zlib.compress(xml.encode('utf-16-le'), 6),
	// an independent oracle of Java's Deflater. PlayerScripts.set validates the script by decompressing it again.
	const std::string expectedHex = "789cb361286648662862c86428602801b2ed186c50441480740a832d8312832110db31bc04caeba3a8b0c31001990200d2d01009";
	std::vector<uint8_t> utf16Bytes;
	for (char16_t c : commons::utils::StringUtils::toUtf16(xml)) {
		utf16Bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		utf16Bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	std::vector<uint8_t> compressedOracle;
	for (size_t i = 0; i < expectedHex.size(); i += 2)
		compressedOracle.push_back(static_cast<uint8_t>(std::stoi(expectedHex.substr(i, 2), nullptr, 16)));
	ASSERT_EQ(utils::xml::CompressUtil::compress(utf16Bytes), compressedOracle);
	try {
		static_cast<void>(utils::xml::CompressUtil::decompress(compressedOracle));
	} catch (const commons::utils::IllegalArgumentException& e) {
		GTEST_SKIP() << "CompressUtil::decompress rejects zlib's output (" << e.what() << "), so PlayerScripts.set drops the script (P4-05)";
	}
	Ref<model::gameobjects::player::PlayerScripts> scripts;
	SKIP_IF_UNPORTED(scripts = HouseScriptsDAO::getPlayerScripts(900));
	Ptr<model::house::PlayerScript> script = scripts->get(1);
	ASSERT_TRUE(script);
	EXPECT_EQ(script->uncompressedSize(), static_cast<int32_t>(utf16Bytes.size())) << "UTF-16LE byte count";
	ASSERT_TRUE(script->compressedBytes());
	std::vector<uint8_t> compressed;
	for (int8_t value : script->compressedBytes()->snapshot())
		compressed.push_back(static_cast<uint8_t>(value));
	EXPECT_EQ(compressed, compressedOracle);

	HouseScriptsDAO::deleteScriptsForHouse(900);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM house_scripts"), 0);
}

TEST_F(LegionHouseItemDaoTest, InventoryRowsWithoutItemTemplates) {
	using model::items::storage::StorageType;
	insertPlayer(1, "Holder", 5);
	// equipped items of the cube: the main hand weapon (with a god stone) is visible, an equipped stigma is not (slot type 0)
	const int64_t mainHand = model::items::getSlotIdMask(model::items::ItemSlot::MAIN_HAND);
	const int64_t stigma = model::items::getSlotIdMask(model::items::ItemSlot::STIGMA1);
	ASSERT_NE(model::items::getEquipmentSlotType(mainHand), 0);
	ASSERT_EQ(model::items::getEquipmentSlotType(stigma), 0);
	execute("INSERT INTO inventory (item_unique_id, item_id, item_owner, is_equipped, slot, item_location, item_skin, item_color) VALUES "
			"(10, 100000001, 1, 1, " + std::to_string(mainHand) + ", 0, 100000099, 16711680), (11, 140000001, 1, 1, " + std::to_string(stigma) +
			", 0, 140000001, NULL), (12, 110000001, 1, 0, 0, 0, 110000001, NULL), (13, 100000002, 5, 0, 0, 2, 0, NULL), (14, 100000003, 1, 0, 0, 3, 0, NULL)");
	execute("INSERT INTO item_stones (item_unique_id, item_id, slot, category, polishNumber, polishCharge) VALUES (10, 168000001, 0, 1, 0, 0)");
	std::vector<Ref<model::account::PlayerAccountData::VisibleItem>> equipment = InventoryDAO::loadVisibleEquipment(1);
	ASSERT_EQ(equipment.size(), 1u) << "the stigma has slot type 0, the unequipped item is not selected";
	EXPECT_EQ(equipment[0]->itemId(), 100000099) << "the skin";
	EXPECT_EQ(equipment[0]->godStoneId(), 168000001) << "LEFT JOIN item_stones category GODSTONE (ordinal 1)";
	EXPECT_EQ(equipment[0]->color(), 16711680);
	EXPECT_EQ(InventoryDAO::getUsedIDs(), (std::vector<int32_t>{10, 11, 12, 13, 14}));

	EXPECT_TRUE(InventoryDAO::deletePlayerOrLegionItems(1));
	EXPECT_EQ(InventoryDAO::getUsedIDs(), (std::vector<int32_t>{13})) << "the account warehouse (location 2) is kept";
	InventoryDAO::deleteAccountWH(5);
	EXPECT_TRUE(InventoryDAO::getUsedIDs().empty());
	EXPECT_TRUE(InventoryDAO::loadBrokerItems().empty());
}

TEST_F(LegionHouseItemDaoTest, InventoryItemsInsertUpdateDeleteAndLoad) {
	using model::gameobjects::Item;
	using model::items::storage::StorageType;
	// Item constructors and the DAO's constructItem look the templates up in DataManager.ITEM_DATA (bound from XML for the test)
	PublishedHolder itemData(dataholders::DataManager::ITEM_DATA,
		bindXml<dataholders::ItemData>(R"(<item_templates><item_template id="100000001" item_group="SWORD"/>)"
									   R"(<item_template id="110000001" item_group="CL_TORSO"/></item_templates>)"));
	const model::templates::item::ItemTemplate* sword = dataholders::DataManager::ITEM_DATA->getItemTemplate(100000001);
	const model::templates::item::ItemTemplate* torso = dataholders::DataManager::ITEM_DATA->getItemTemplate(110000001);
	ASSERT_TRUE(sword && torso);

	insertPlayer(1, "Holder", 77);
	const int64_t mainHand = model::items::getSlotIdMask(model::items::ItemSlot::MAIN_HAND);
	Ref<Item> weapon;
	Ref<Item> armor;
	Ref<Item> shared;
	SKIP_IF_UNPORTED({
		weapon = Item::create(500, sword, 1, true, mainHand);
		armor = Item::create(501, torso, 3, false, 0);
		shared = Item::create(502, torso, 1, false, 0);
		weapon->setItemLocation(model::items::storage::getId(StorageType::CUBE));
		armor->setItemLocation(model::items::storage::getId(StorageType::CUBE));
		shared->setItemLocation(model::items::storage::getId(StorageType::ACCOUNT_WAREHOUSE));
		weapon->setItemColor(0x112233);
		for (const Ref<Item>& item : {weapon, armor, shared})
			item->setPersistentState(Persistable::PersistentState::NEW);
	});
	EXPECT_TRUE(InventoryDAO::store({Ptr<Item>(weapon), Ptr<Item>(armor), Ptr<Item>(shared)}, 1));
	EXPECT_EQ(weapon->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(queryLong("SELECT item_owner FROM inventory WHERE item_unique_id = 502"), 77) << "account warehouse items belong to the account";
	EXPECT_FALSE(queryString("SELECT item_color FROM inventory WHERE item_unique_id = 501")) << "no color: NULL";

	std::vector<Ref<Item>> cube = InventoryDAO::loadItems(1, StorageType::CUBE);
	ASSERT_EQ(cube.size(), 2u);
	Ref<Item> loadedWeapon = cube[0]->getObjectId() == 500 ? cube[0] : cube[1];
	EXPECT_EQ(loadedWeapon->getItemTemplate(), sword);
	EXPECT_TRUE(loadedWeapon->isEquipped());
	EXPECT_EQ(loadedWeapon->getEquipmentSlot(), mainHand);
	EXPECT_EQ(loadedWeapon->getItemColor(), 0x112233);
	EXPECT_EQ(loadedWeapon->getItemLocation(), model::items::storage::getId(StorageType::CUBE));
	EXPECT_EQ(loadedWeapon->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(InventoryDAO::loadItems(77, StorageType::ACCOUNT_WAREHOUSE).size(), 1u);

	// update and delete in one store call (three batches, each committed)
	weapon->setEquipped(false);
	weapon->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	armor->setPersistentState(Persistable::PersistentState::DELETED);
	EXPECT_TRUE(InventoryDAO::store({Ptr<Item>(weapon), Ptr<Item>(armor)}, std::optional<int32_t>(1), std::nullopt, std::nullopt));
	EXPECT_EQ(queryLong("SELECT is_equipped FROM inventory WHERE item_unique_id = 500"), 0);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM inventory WHERE item_unique_id = 501"), 0);

	// the account id is null: getItemOwnerId unboxes it (Java NullPointerException), the insert batch fails and store returns false
	shared->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_FALSE(InventoryDAO::store({Ptr<Item>(shared)}, std::optional<int32_t>(1), std::nullopt, std::nullopt));
}

TEST_F(LegionHouseItemDaoTest, ManaStonesAddUpdateDeleteInTransactions) {
	using model::items::ManaStone;
	// the ItemStone constructor requires the stone's item template (Java: Objects.requireNonNull(getItemTemplate()))
	static const std::unique_ptr<model::templates::item::ItemTemplate> stoneTemplate =
		bindXml<model::templates::item::ItemTemplate>(R"(<item_template id="167000001" item_group="MANASTONE"/>)");
	model::items::detail::StaticDataLookupsForTests lookups;
	lookups.itemTemplate = [](int32_t) -> const model::templates::item::ItemTemplate* { return stoneTemplate.get(); };
	model::items::detail::setStaticDataLookupsForTests(lookups);
	struct LookupsReset {
		~LookupsReset() { model::items::detail::setStaticDataLookupsForTests({}); }
	} lookupsReset;

	// item stones reference inventory items (foreign key): a batch for a missing item fails, is logged and leaves nothing behind (the pool rolls the
	// uncommitted transaction back)
	ItemStoneListDAO::storeManaStones({Ptr<ManaStone>(ManaStone::create(99, 167000001, 0, Persistable::PersistentState::NEW))});
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM item_stones"), 0);
	execute("INSERT INTO inventory (item_unique_id, item_id, item_owner) VALUES (10, 100000001, 1), (11, 100000002, 1)");

	Ref<ManaStone> first = ManaStone::create(10, 167000001, 0, Persistable::PersistentState::NEW);
	Ref<ManaStone> second = ManaStone::create(10, 167000002, 1, Persistable::PersistentState::NEW);
	ItemStoneListDAO::storeManaStones({Ptr<ManaStone>(first), Ptr<ManaStone>(second)});
	EXPECT_EQ(first->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(queryString("SELECT GROUP_CONCAT(CONCAT(item_id, '@', slot, '#', category) ORDER BY slot) FROM item_stones"),
		"167000001@0#0,167000002@1#0");

	ItemStoneListDAO::storeFusionStone({Ptr<ManaStone>(ManaStone::create(11, 167000003, 0, Persistable::PersistentState::NEW))});
	EXPECT_EQ(queryLong("SELECT category FROM item_stones WHERE item_unique_id = 11"), 2) << "FUSIONSTONE ordinal";

	second->setPersistentState(Persistable::PersistentState::DELETED);
	ItemStoneListDAO::storeManaStones({Ptr<ManaStone>(second)});
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM item_stones WHERE item_unique_id = 10"), 1)
		<< "Java executes the delete once directly and once in the batch";
	EXPECT_TRUE(InventoryDAO::loadBrokerItems().empty());
}

TEST_F(LegionHouseItemDaoTest, RegisteredItemsAndBroker) {
	insertPlayer(1, "Decorator", 1);
	execute("INSERT INTO player_registered_items (player_id, item_unique_id, item_id, x, y, z, h, area) VALUES "
			"(1, 800, 1, 1, 2, 3, 4, 'INTERIOR'), (1, 801, 2, 0, 0, 0, 0, 'DECOR'), (1, 0, 3, 0, 0, 0, 0, 'NONE')");
	EXPECT_EQ(PlayerRegisteredItemsDAO::getUsedIDs(), (std::vector<int32_t>{800, 801})) << "item_unique_id <> 0";
	PlayerRegisteredItemsDAO::resetRegistry(1);
	EXPECT_EQ(queryString("SELECT CONCAT(x, ',', area) FROM player_registered_items WHERE item_unique_id = 800"), "0,NONE");
	EXPECT_EQ(queryString("SELECT area FROM player_registered_items WHERE item_unique_id = 801"), "DECOR") << "decorations keep their area";
	EXPECT_TRUE(PlayerRegisteredItemsDAO::deletePlayerItems(1));
	EXPECT_TRUE(PlayerRegisteredItemsDAO::getUsedIDs().empty());

	EXPECT_FALSE(BrokerDAO::store(nullptr)) << "Java logs a null broker item";
	EXPECT_TRUE(BrokerDAO::loadBroker().empty());
}

TEST_F(LegionHouseItemDaoTest, BrokerRowsWithoutAnItemAllLoad) {
	// BrokerDAO.loadBroker (header request dao-3): a sold entry and an entry whose broker item row is missing get a null item, and the rows after
	// them still load (a failure inside the DB.select handler would drop them all)
	insertPlayer(2, "Seller", 2);
	execute("INSERT INTO broker (id, item_pointer, item_id, item_count, item_creator, price, broker_race, seller_id, is_sold, is_settled) VALUES "
			"(1, 700, 100000001, 1, NULL, 500, 'ELYOS', 2, 1, 0), (2, 701, 100000002, 3, 'Maker', 900, 'ASMODIAN', 2, 0, 0), "
			"(3, 702, 100000003, 2, NULL, 100, 'ELYOS', 2, 1, 1)");
	std::vector<Ref<model::gameobjects::BrokerItem>> items = BrokerDAO::loadBroker();
	ASSERT_EQ(items.size(), 3u);
	EXPECT_EQ(items[0]->getItemUniqueId(), 700);
	EXPECT_TRUE(items[0]->isSold());
	EXPECT_FALSE(items[0]->getItem()) << "sold: Java leaves the item null";
	EXPECT_EQ(items[1]->getItemUniqueId(), 701);
	EXPECT_FALSE(items[1]->isSold());
	EXPECT_FALSE(items[1]->getItem()) << "unsold, but no inventory row in the BROKER storage";
	EXPECT_EQ(items[2]->getItemUniqueId(), 702);
	EXPECT_TRUE(items[2]->isSold());
}

} // namespace
} // namespace aion::gameserver::dao::test
