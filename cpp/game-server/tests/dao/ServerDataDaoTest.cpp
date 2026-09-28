// Round trips of the DAOs that need no player object (P4-14) against a fresh aion_gs.sql database (DaoTestDatabase.h): server variables,
// packs, old names, surveys, web rewards (batch in a transaction), recipes, bookmarks, announcements (generated keys), custom instance ranks,
// command accesses, towns, headhunters, macros, passkeys, guides (scrollable getUsedIDs), passports and stamps, event buff data.

#include <gtest/gtest.h>

#include <any>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "DaoTestSupport.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/custom/instance/CustomInstanceRank.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankedPlayer.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TownSpawnsData.bind.h"
#include "aion/gameserver/dataholders/TownSpawnsData.h"
#include "aion/gameserver/dao/AccountPassportsDAO.h"
#include "aion/gameserver/dao/AnnouncementsDAO.h"
#include "aion/gameserver/dao/BonusPackDAO.h"
#include "aion/gameserver/dao/BookmarkDAO.h"
#include "aion/gameserver/dao/CommandsAccessDAO.h"
#include "aion/gameserver/dao/CustomInstanceDAO.h"
#include "aion/gameserver/dao/EventDAO.h"
#include "aion/gameserver/dao/FactionPackDAO.h"
#include "aion/gameserver/dao/GuideDAO.h"
#include "aion/gameserver/dao/HeadhuntingDAO.h"
#include "aion/gameserver/dao/OldNamesDAO.h"
#include "aion/gameserver/dao/PlayerMacrosDAO.h"
#include "aion/gameserver/dao/PlayerPasskeyDAO.h"
#include "aion/gameserver/dao/PlayerRecipesDAO.h"
#include "aion/gameserver/dao/RewardServiceDAO.h"
#include "aion/gameserver/dao/ServerVariablesDAO.h"
#include "aion/gameserver/dao/SurveyControllerDAO.h"
#include "aion/gameserver/dao/TownDAO.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/Announcement.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/guide/Guide.h"
#include "aion/gameserver/model/templates/rewards/RewardEntryItem.h"
#include "aion/gameserver/model/templates/survey/SurveyItem.h"
#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao::test {
namespace {

using model::gameobjects::Persistable;
using runtime::Ptr;
using runtime::Ref;

class ServerDataDaoTest : public DaoTest {};

TEST_F(ServerDataDaoTest, ServerVariablesStoreLoadAndDelete) {
	EXPECT_FALSE(ServerVariablesDAO::loadInt("time"));
	EXPECT_TRUE(ServerVariablesDAO::store("time", std::any(int32_t{1234})));
	EXPECT_TRUE(ServerVariablesDAO::store("serverLastRun", std::any(int64_t{1757894400123})));
	EXPECT_EQ(ServerVariablesDAO::loadInt("time"), 1234);
	EXPECT_EQ(ServerVariablesDAO::loadLong("serverLastRun"), 1757894400123);
	EXPECT_EQ(queryString("SELECT `value` FROM server_variables WHERE `key` = 'serverLastRun'"), "1757894400123") << "Java Long.toString";
	EXPECT_TRUE(ServerVariablesDAO::store("time", std::any(int32_t{-5})));
	EXPECT_EQ(ServerVariablesDAO::loadInt("time"), -5) << "REPLACE INTO";
	EXPECT_EQ(ServerVariablesDAO::loadLong("time"), -5);

	ServerVariablesDAO dao; // Java: delete is an instance method
	EXPECT_TRUE(dao.delete_("time"));
	EXPECT_FALSE(dao.delete_("time"));
	EXPECT_FALSE(ServerVariablesDAO::loadInt("time"));
	EXPECT_THROW(ServerVariablesDAO::store("empty", std::any()), runtime::NullPointerException) << "Java: value.toString() on null";

	EXPECT_TRUE(ServerVariablesDAO::store("text", std::any(std::string("abc"))));
	EXPECT_THROW(static_cast<void>(ServerVariablesDAO::loadInt("text")), commons::utils::NumberFormatException) << "Java Integer.parseInt";
}

TEST_F(ServerDataDaoTest, BonusAndFactionPacksReplaceTheReceivingPlayer) {
	EXPECT_EQ(BonusPackDAO::loadReceivingPlayer(7), 0);
	EXPECT_TRUE(BonusPackDAO::storeReceivingPlayer(7, 100));
	EXPECT_TRUE(BonusPackDAO::storeReceivingPlayer(7, 101));
	EXPECT_EQ(BonusPackDAO::loadReceivingPlayer(7), 101);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM bonus_packs"), 1);

	EXPECT_EQ(FactionPackDAO::loadReceivingPlayer(7), 0);
	EXPECT_TRUE(FactionPackDAO::storeReceivingPlayer(7, 200));
	EXPECT_EQ(FactionPackDAO::loadReceivingPlayer(7), 200);
	EXPECT_EQ(BonusPackDAO::loadReceivingPlayer(7), 101);
}

TEST_F(ServerDataDaoTest, OldNamesReservation) {
	insertPlayer(1, "Newname", 1);
	OldNamesDAO::insertNames(1, "Oldname", "Newname");
	EXPECT_EQ(queryString("SELECT CONCAT(old_name, '>', new_name) FROM old_names WHERE player_id = 1"), "Oldname>Newname");
	// the old name stays reserved for other characters, but the character that gave it up may take it back
	EXPECT_TRUE(OldNamesDAO::isNameReserved(std::nullopt, "Oldname", 30)) << "a new character (old name null)";
	EXPECT_TRUE(OldNamesDAO::isNameReserved(std::string_view("Othername"), "Oldname", 30));
	EXPECT_FALSE(OldNamesDAO::isNameReserved(std::string_view("Newname"), "Oldname", 30)) << "renaming back";
	EXPECT_FALSE(OldNamesDAO::isNameReserved(std::nullopt, "Oldname", 0)) << "no reservation period";
	EXPECT_FALSE(OldNamesDAO::isNameReserved(std::nullopt, "Unused", 30));
	execute("UPDATE old_names SET renamed_date = NOW() - INTERVAL 40 DAY");
	EXPECT_FALSE(OldNamesDAO::isNameReserved(std::nullopt, "Oldname", 30)) << "the reservation expired";
}

TEST_F(ServerDataDaoTest, SurveysAreLoadedUntilUsed) {
	insertPlayer(1, "Surveyor", 1);
	execute("INSERT INTO surveys (unique_id, owner_id, item_id, item_count, html_text, html_radio) VALUES (5, 1, 186000030, 3, '<html/>', 'accept')");
	execute("INSERT INTO surveys (unique_id, owner_id, item_id, html_text, used) VALUES (6, 1, 186000031, 'used', 1)");
	std::vector<Ref<model::templates::survey::SurveyItem>> unused = SurveyControllerDAO::getAllUnused();
	ASSERT_EQ(unused.size(), 1u);
	EXPECT_EQ(unused[0]->uniqueId.get(), 5);
	EXPECT_EQ(unused[0]->ownerId.get(), 1);
	EXPECT_EQ(unused[0]->itemId.get(), 186000030);
	EXPECT_EQ(unused[0]->count.get(), 3);
	EXPECT_EQ(unused[0]->html.get(), "<html/>");
	EXPECT_EQ(unused[0]->radio.get(), "accept");
	EXPECT_TRUE(SurveyControllerDAO::useItem(5));
	EXPECT_TRUE(SurveyControllerDAO::getAllUnused().empty());
	EXPECT_NE(queryString("SELECT used_time FROM surveys WHERE unique_id = 5"), "") << "used_time=NOW()";
}

TEST_F(ServerDataDaoTest, WebRewardsBatchUpdateInOneTransaction) {
	insertPlayer(1, "Rewarded", 1);
	insertPlayer(2, "Other", 2);
	execute("INSERT INTO player_web_rewards (entry_id, player_id, item_id, item_count) VALUES (1, 1, 100, 2), (2, 1, 101, 5000000000), (3, 2, 102, 1)");
	std::vector<Ref<model::templates::rewards::RewardEntryItem>> rewards = RewardServiceDAO::loadUnreceived(1);
	ASSERT_EQ(rewards.size(), 2u);
	EXPECT_EQ(rewards[0]->getEntryId(), 1);
	EXPECT_EQ(rewards[1]->getCount(), 5000000000);

	const int64_t received = 1757894400000; // 2025-09-15 00:00:00 UTC
	RewardServiceDAO::storeReceived({1, 2}, received);
	EXPECT_TRUE(RewardServiceDAO::loadUnreceived(1).empty());
	EXPECT_EQ(RewardServiceDAO::loadUnreceived(2).size(), 1u);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM player_web_rewards WHERE received IS NOT NULL"), 2) << "the batch was committed";
	EXPECT_EQ(queryLong("SELECT UNIX_TIMESTAMP(received) FROM player_web_rewards WHERE entry_id = 2"), received / 1000);
}

TEST_F(ServerDataDaoTest, RecipesAddLoadAndDelete) {
	insertPlayer(1, "Crafter", 1);
	EXPECT_TRUE(PlayerRecipesDAO::addRecipe(1, 155000001));
	EXPECT_TRUE(PlayerRecipesDAO::addRecipe(1, 155000002));
	EXPECT_FALSE(PlayerRecipesDAO::addRecipe(1, 155000002)) << "duplicate primary key: DB.insertUpdate logs and returns false";
	Ref<model::gameobjects::player::RecipeList> recipes = PlayerRecipesDAO::load(1);
	EXPECT_EQ(recipes->getRecipeList().size(), 2);
	EXPECT_TRUE(recipes->getRecipeList().contains(155000001));
	EXPECT_TRUE(PlayerRecipesDAO::delRecipe(1, 155000001));
	EXPECT_EQ(PlayerRecipesDAO::load(1)->getRecipeList().size(), 1);
	EXPECT_EQ(PlayerRecipesDAO::load(2)->getRecipeList().size(), 0);
}

TEST_F(ServerDataDaoTest, BookmarksReplaceLoadAndDelete) {
	insertPlayer(1, "Traveler", 1);
	BookmarkDAO::storeBookmark(1, *BookmarkDAO::Bookmark::create("Home", 110010000, 1.5f, -2.25f, 100.0f));
	BookmarkDAO::storeBookmark(1, *BookmarkDAO::Bookmark::create("Mine", 210010000, 3.0f, 4.0f, 5.0f));
	BookmarkDAO::storeBookmark(1, *BookmarkDAO::Bookmark::create("Home", 110010000, 7.0f, 8.0f, 9.0f)); // REPLACE INTO
	std::vector<Ref<BookmarkDAO::Bookmark>> bookmarks = BookmarkDAO::loadBookmarks(1);
	ASSERT_EQ(bookmarks.size(), 2u);
	for (const Ref<BookmarkDAO::Bookmark>& bookmark : bookmarks) {
		if (bookmark->name() == "Home")
			EXPECT_TRUE(bookmark->equals(*BookmarkDAO::Bookmark::create("Home", 110010000, 7.0f, 8.0f, 9.0f)));
		else
			EXPECT_TRUE(bookmark->equals(*BookmarkDAO::Bookmark::create("Mine", 210010000, 3.0f, 4.0f, 5.0f)));
	}
	EXPECT_TRUE(BookmarkDAO::deleteBookmark(1, "Home"));
	EXPECT_FALSE(BookmarkDAO::deleteBookmark(1, "Home")) << "executeUpdate() > 0";
	BookmarkDAO::deleteAll(1);
	EXPECT_TRUE(BookmarkDAO::loadBookmarks(1).empty());
}

TEST_F(ServerDataDaoTest, AnnouncementsReturnGeneratedKeys) {
	const int32_t first = AnnouncementsDAO::addAnnouncement("Line one\\nLine two\\tTabbed", "ALL", "SYSTEM", 600);
	const int32_t second = AnnouncementsDAO::addAnnouncement("Elyos only", "ELYOS", "SHOUT", 30);
	EXPECT_GT(first, 0);
	EXPECT_EQ(second, first + 1) << "AUTO_INCREMENT key of the second insert";
	std::vector<Ref<model::Announcement>> announcements = AnnouncementsDAO::loadAnnouncements();
	ASSERT_EQ(announcements.size(), 2u);
	EXPECT_EQ(announcements[0]->getId(), first) << "ORDER BY id";
	EXPECT_EQ(announcements[0]->getAnnounce(), "Line one\nLine two\tTabbed") << "the escaped \\n and \\t are replaced";
	EXPECT_EQ(announcements[0]->getDelay(), 600);
	EXPECT_FALSE(announcements[0]->getFaction());
	EXPECT_EQ(announcements[1]->getFaction(), model::Race::ELYOS);
	EXPECT_TRUE(AnnouncementsDAO::delAnnouncement(first));
	EXPECT_EQ(AnnouncementsDAO::loadAnnouncements().size(), 1u);
	EXPECT_EQ(AnnouncementsDAO::addAnnouncement("bad", "NOBODY", "SYSTEM", 1), -1) << "an invalid enum value fails the insert (strict mode)";
}

TEST_F(ServerDataDaoTest, CustomInstanceRanksAndTop10) {
	insertPlayer(1, "Ranked", 1, "ELYOS", 0, "TEMPLAR");
	insertPlayer(2, "Asmo", 2, "ASMODIANS");
	EXPECT_FALSE(CustomInstanceDAO::loadPlayerRankObject(1));
	const int64_t lastEntry = (commons::utils::currentTimeMillis() / 1000 - 3600) * 1000;
	custom::instance::CustomInstanceRank rank(1, 5, lastEntry, 7, 12345);
	EXPECT_TRUE(CustomInstanceDAO::storePlayer(rank));
	std::optional<custom::instance::CustomInstanceRank> loaded = CustomInstanceDAO::loadPlayerRankObject(1);
	ASSERT_TRUE(loaded);
	EXPECT_EQ(loaded->getRank(), 5);
	EXPECT_EQ(loaded->getLastEntry(), lastEntry);
	EXPECT_EQ(loaded->getMaxRank(), 7);
	EXPECT_EQ(loaded->getDps(), 12345);
	custom::instance::CustomInstanceRank asmoRank(2, 9, lastEntry, 9, 1);
	EXPECT_TRUE(CustomInstanceDAO::storePlayer(asmoRank));

	std::vector<custom::instance::CustomInstanceRankedPlayer> top = CustomInstanceDAO::loadTop10(model::Race::ELYOS);
	ASSERT_EQ(top.size(), 1u);
	EXPECT_EQ(top[0].getPlayerId(), 1);
	EXPECT_EQ(top[0].getName(), "Ranked");
	EXPECT_EQ(top[0].getPlayerClass(), model::PlayerClass::TEMPLAR);
	EXPECT_EQ(top[0].getLastEntry(), lastEntry);
	execute("UPDATE custom_instance SET last_entry = NOW() - INTERVAL 20 DAY WHERE player_id = 1");
	EXPECT_TRUE(CustomInstanceDAO::loadTop10(model::Race::ELYOS).empty()) << "only entries of the last 14 days";
}

TEST_F(ServerDataDaoTest, CommandAccesses) {
	insertPlayer(1, "Admin", 1);
	insertPlayer(2, "Helper", 2);
	CommandsAccessDAO::addAccess(1, "spawn");
	CommandsAccessDAO::addAccess(1, "move");
	CommandsAccessDAO::addAccess(2, "move");
	auto accesses = CommandsAccessDAO::loadAccesses();
	ASSERT_EQ(accesses.size(), 2u);
	EXPECT_EQ(accesses[1], (std::unordered_set<std::string>{"spawn", "move"}));
	EXPECT_EQ(accesses[2], (std::unordered_set<std::string>{"move"}));
	CommandsAccessDAO::removeAccess(1, "spawn");
	CommandsAccessDAO::removeAllAccesses(2);
	accesses = CommandsAccessDAO::loadAccesses();
	ASSERT_EQ(accesses.size(), 1u);
	EXPECT_EQ(accesses[1], (std::unordered_set<std::string>{"move"}));
}

TEST_F(ServerDataDaoTest, TownsAreLoadedByRace) {
	// the Town constructor spawns its town objects: Java needs a spawn map entry per town and level (an empty level spawns nothing)
	PublishedHolder townSpawns(dataholders::DataManager::TOWN_SPAWNS_DATA, bindXml<dataholders::TownSpawnsData>(
		R"(<town_spawns_data><spawn_map map_id="700010000"><town_spawn town_id="1001"><town_level level="3"/></town_spawn>)"
		R"(<town_spawn town_id="2001"><town_level level="1"/></town_spawn></spawn_map></town_spawns_data>)"));
	execute("INSERT INTO towns (id, level, points, race, level_up_date) VALUES (1001, 3, 450, 'ELYOS', '2025-09-15 12:34:56'), (2001, 1, 0, 'ASMODIANS', DEFAULT)");
	std::unordered_map<int32_t, Ref<model::town::Town>> towns;
	// Java spawns the town objects in the Town constructor, which needs a loaded GeoService; the geo fixture belongs to the
	// stage-2 scenario work (m5a-plan.md X-02), so skip the row assertions until then instead of loading 156 MB of geo data here
	try {
		SKIP_IF_UNPORTED(towns = TownDAO::load(model::Race::ELYOS));
	} catch (const commons::utils::Exception& e) {
		GTEST_SKIP() << "town spawning needs a loaded GeoService: " << e.what();
	}
	ASSERT_EQ(towns.size(), 1u);
	Ref<model::town::Town> town = towns.at(1001);
	EXPECT_EQ(town->getLevel(), 3);
	EXPECT_EQ(town->getPoints(), 450);
	EXPECT_EQ(town->getRace(), model::Race::ELYOS);
	ASSERT_TRUE(town->getLevelUpDate());
	EXPECT_EQ(queryLong("SELECT UNIX_TIMESTAMP(level_up_date) FROM towns WHERE id = 1001"), detail::getTime(*town->getLevelUpDate()) / 1000);
	EXPECT_EQ(TownDAO::load(model::Race::ASMODIANS).size(), 1u);
}

TEST_F(ServerDataDaoTest, HeadhuntersLoadInIdOrderAndClear) {
	execute("INSERT INTO headhunting (hunter_id, accumulated_kills, last_update) VALUES (30, 5, '2025-09-01 10:00:00'), (10, 7, '2025-09-02 10:00:00')");
	std::map<int32_t, Ref<model::event::Headhunter>> hunters = HeadhuntingDAO::loadHeadhunters();
	ASSERT_EQ(hunters.size(), 2u);
	EXPECT_EQ(hunters.begin()->first, 10) << "TreeMap order";
	EXPECT_EQ(hunters.at(30)->getKills(), 5);
	EXPECT_EQ(hunters.at(10)->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(hunters.at(10)->getLastUpdate(), *queryLong("SELECT UNIX_TIMESTAMP(last_update) * 1000 FROM headhunting WHERE hunter_id = 10"));
	EXPECT_TRUE(HeadhuntingDAO::clearTables());
	EXPECT_TRUE(HeadhuntingDAO::loadHeadhunters().empty());
}

TEST_F(ServerDataDaoTest, MacrosAddUpdateLoadDelete) {
	insertPlayer(1, "Macro", 1);
	PlayerMacrosDAO::addMacro(1, 1, "<macro>one</macro>");
	PlayerMacrosDAO::addMacro(1, 2, "<macro>two</macro>");
	PlayerMacrosDAO::updateMacro(1, 2, "<macro>changed</macro>");
	PlayerMacrosDAO::deleteMacro(1, 1);
	Ref<model::gameobjects::player::Macros> macros = PlayerMacrosDAO::loadMacros(1);
	std::vector<Ptr<model::gameobjects::player::Macros::Macro>> all = macros->getAll();
	ASSERT_EQ(all.size(), 1u);
	EXPECT_EQ(all[0]->id(), 2);
	EXPECT_EQ(all[0]->xml(), "<macro>changed</macro>");
}

TEST_F(ServerDataDaoTest, Passkeys) {
	EXPECT_FALSE(PlayerPasskeyDAO::existCheckPlayerPasskey(3));
	PlayerPasskeyDAO::insertPlayerPasskey(3, "1234");
	EXPECT_TRUE(PlayerPasskeyDAO::existCheckPlayerPasskey(3));
	EXPECT_TRUE(PlayerPasskeyDAO::checkPlayerPasskey(3, "1234"));
	EXPECT_FALSE(PlayerPasskeyDAO::checkPlayerPasskey(3, "0000"));
	EXPECT_FALSE(PlayerPasskeyDAO::updatePlayerPasskey(3, "0000", "5678")) << "wrong old passkey";
	EXPECT_TRUE(PlayerPasskeyDAO::updatePlayerPasskey(3, "1234", "5678"));
	EXPECT_TRUE(PlayerPasskeyDAO::checkPlayerPasskey(3, "5678"));
	EXPECT_TRUE(PlayerPasskeyDAO::updateForcePlayerPasskey(3, "9999"));
	EXPECT_FALSE(PlayerPasskeyDAO::updateForcePlayerPasskey(4, "9999"));
	EXPECT_TRUE(PlayerPasskeyDAO::checkPlayerPasskey(3, "9999"));
}

TEST_F(ServerDataDaoTest, GuidesAndScrollableUsedIds) {
	insertPlayer(1, "Guided", 1);
	auto f = makePlayer(1, 1, "Guided");
	GuideDAO::saveGuide(500, *f.player, "First guide");
	GuideDAO::saveGuide(501, *f.player, "Second guide");
	std::vector<model::guide::Guide> guides = GuideDAO::loadGuides(1);
	ASSERT_EQ(guides.size(), 2u);
	EXPECT_EQ(guides[0].getTitle(), "First guide");
	std::optional<model::guide::Guide> guide = GuideDAO::loadGuide(1, 501);
	ASSERT_TRUE(guide);
	EXPECT_EQ(guide->getGuideId(), 501);
	EXPECT_EQ(guide->getTitle(), "Second guide");
	EXPECT_FALSE(GuideDAO::loadGuide(2, 501)) << "another player's guide";
	EXPECT_EQ(GuideDAO::getUsedIDs(), (std::vector<int32_t>{500, 501}));
	EXPECT_TRUE(GuideDAO::deleteGuide(500));
	EXPECT_EQ(GuideDAO::getUsedIDs(), (std::vector<int32_t>{501}));
}

TEST_F(ServerDataDaoTest, UsedIdsOfAMissingTableThrowLikeTheJavaNull) {
	execute("RENAME TABLE guides TO guides_hidden");
	EXPECT_THROW(static_cast<void>(GuideDAO::getUsedIDs()), runtime::NullPointerException)
		<< "Java returns null, and IDFactory.lockIds throws a NullPointerException";
	execute("RENAME TABLE guides_hidden TO guides");
	EXPECT_TRUE(GuideDAO::getUsedIDs().empty());
}

TEST_F(ServerDataDaoTest, PassportsAndStamps) {
	Ref<model::account::Account> account = model::account::Account::create(42);
	AccountPassportsDAO::loadPassport(*account);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM account_stamps WHERE account_id = 42"), 1) << "missing stamps are inserted";
	ASSERT_TRUE(account->getPassportsList());
	EXPECT_EQ(account->getPassportsList()->getAllPassports().size(), 0);
	EXPECT_EQ(account->getPassportStamps(), 0);
	EXPECT_FALSE(account->getLastStamp());

	// a new passport and new stamps (whole seconds: the TIMESTAMP columns have no fraction)
	const commons::database::Timestamp arrive = detail::toTimestamp(1757894401000);
	Ref<model::account::Passport> passport = model::account::Passport::create(3, false, arrive);
	passport->setPersistentState(Persistable::PersistentState::NEW);
	account->getPassportsList()->addPassport(*passport);
	account->setPassportStamps(4);
	account->setLastStamp(arrive);
	AccountPassportsDAO::storePassport(*account);
	EXPECT_EQ(passport->getPersistentState(), Persistable::PersistentState::UPDATED);

	Ref<model::account::Account> reloaded = model::account::Account::create(42);
	AccountPassportsDAO::loadPassport(*reloaded);
	std::vector<Ptr<model::account::Passport>> passports = reloaded->getPassportsList()->getAllPassports().snapshot();
	ASSERT_EQ(passports.size(), 1u);
	EXPECT_EQ(passports[0]->getId(), 3);
	EXPECT_FALSE(passports[0]->isRewarded());
	EXPECT_EQ(passports[0]->getArriveDate(), arrive);
	EXPECT_EQ(passports[0]->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_EQ(reloaded->getPassportStamps(), 4);
	EXPECT_EQ(reloaded->getLastStamp(), arrive);

	// rewarded update, then delete
	Ref<model::account::Passport> rewarded = model::account::Passport::create(3, true, arrive);
	rewarded->setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	AccountPassportsDAO::storePassportList(42, {Ptr<model::account::Passport>(rewarded)});
	EXPECT_EQ(queryLong("SELECT rewarded FROM account_passports WHERE account_id = 42"), 1);
	rewarded->setPersistentState(Persistable::PersistentState::DELETED);
	AccountPassportsDAO::storePassportList(42, {Ptr<model::account::Passport>(rewarded)});
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM account_passports"), 0);

	AccountPassportsDAO::resetAllLastStamps();
	AccountPassportsDAO::resetAllStamps();
	EXPECT_EQ(queryLong("SELECT stamps FROM account_stamps WHERE account_id = 42"), 0);
	EXPECT_FALSE(queryString("SELECT last_stamp FROM account_stamps WHERE account_id = 42")) << "last_stamp=NULL";
}

TEST_F(ServerDataDaoTest, EventBuffDataRoundTrip) {
	std::optional<std::vector<EventDAO::StoredBuffData>> none = EventDAO::loadStoredBuffData("Event");
	ASSERT_TRUE(none) << "Java: isAfterLast() of an empty result set is false, so an empty list";
	EXPECT_TRUE(none->empty());

	std::vector<EventDAO::StoredBuffData> data;
	data.emplace_back(0, std::unordered_set<int32_t>{16, 1, 17, 3}, std::nullopt);
	data.emplace_back(1, std::unordered_set<int32_t>{2}, std::unordered_set<int32_t>{6, 7});
	EXPECT_TRUE(EventDAO::storeBuffData("Event", data));
	// Java HashSet<Integer> order: the buckets of a 16-slot table, so 16 (bucket 0) comes before 1 and 17 (bucket 1) before 3
	EXPECT_EQ(queryString("SELECT buff_active_pool_ids FROM event WHERE buff_index = 0"), "16,1,17,3");
	EXPECT_FALSE(queryString("SELECT buff_allowed_days FROM event WHERE buff_index = 0")) << "a null set is stored as NULL";
	EXPECT_EQ(queryString("SELECT buff_allowed_days FROM event WHERE buff_index = 1"), "6,7");

	std::vector<EventDAO::StoredBuffData> first;
	first.emplace_back(0, std::unordered_set<int32_t>{16, 1, 17, 3}, std::nullopt);
	EXPECT_TRUE(EventDAO::storeBuffData("Event", first)) << "old entries are deleted first";
	std::optional<std::vector<EventDAO::StoredBuffData>> loaded = EventDAO::loadStoredBuffData("Event");
	ASSERT_TRUE(loaded);
	ASSERT_EQ(loaded->size(), 1u);
	EXPECT_EQ((*loaded)[0].getActivePoolSkillIds(), (std::unordered_set<int32_t>{1, 3, 16, 17}));
	EXPECT_FALSE((*loaded)[0].getAllowedBuffDays());

	// Java: "".split(",") is {""}, and Integer.parseInt("") throws a NumberFormatException (no SQLException, so it propagates)
	std::vector<EventDAO::StoredBuffData> empty;
	empty.emplace_back(0, std::unordered_set<int32_t>{}, std::nullopt);
	EXPECT_TRUE(EventDAO::storeBuffData("Empty", empty));
	EXPECT_THROW(static_cast<void>(EventDAO::loadStoredBuffData("Empty")), commons::utils::NumberFormatException);

	// deleteOldBuffData: rows changed before the first day of the current month (server time zone)
	const std::chrono::time_zone* previousZone = configs::main::GSConfig::TIME_ZONE_ID.load();
	configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone("UTC"));
	execute("UPDATE event SET last_change = NOW() - INTERVAL 40 DAY WHERE event_name = 'Empty'");
	EventDAO::deleteOldBuffData();
	configs::main::GSConfig::TIME_ZONE_ID.store(previousZone);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM event WHERE event_name = 'Empty'"), 0);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM event WHERE event_name = 'Event'"), 1);
}

} // namespace
} // namespace aion::gameserver::dao::test
