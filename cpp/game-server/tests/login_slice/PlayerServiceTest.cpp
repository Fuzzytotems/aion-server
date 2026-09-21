// PlayerService (P5-00, m5a-plan.md S-04/S-09/S-10) on the test schema: name checks, deletion and creation times, the player name lookup, and
// storeNewPlayer/storePlayer of a player built by newPlayer (SlicePlayer: the real Player with stat container doubles).

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "SliceDbTest.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/AccountService.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::loginslice::test {
namespace {

using model::account::Account;
using model::account::PlayerAccountData;
using model::gameobjects::player::Player;
using model::gameobjects::player::PlayerCommonData;
using runtime::Ptr;
using runtime::Ref;
using services::player::PlayerService;

/** Sets an atomic configuration field for the scope and restores the previous value */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

class PlayerServiceTest : public SliceDbTest {
protected:
	/** Java CM_CREATE_CHARACTER: an account with its warehouse, the common data of a level-1 character and its account data */
	struct NewCharacter {
		Ref<Account> account;
		Ref<PlayerCommonData> commonData;
		Ref<model::gameobjects::player::PlayerAppearance> appearance;
		/** a part of the account (added right away: a Player built on it holds a Ref to it, so it must not be destroyed before the player) */
		Ptr<PlayerAccountData> accountData;
	};

	static NewCharacter newCharacter(int32_t objectId, std::string_view name, model::Race race, model::PlayerClass playerClass) {
		NewCharacter c;
		c.account = Account::create(21);
		c.account->setName("account21");
		c.account->setAccountWarehouse(std::make_unique<model::items::storage::PlayerStorage>(*c.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		c.commonData = PlayerCommonData::create(objectId);
		c.commonData->setName(name);
		c.commonData->setGender(model::Gender::FEMALE);
		c.commonData->setRace(race);
		c.commonData->setPlayerClass(playerClass);
		c.commonData->setLevel(1);
		c.appearance = model::gameobjects::player::PlayerAppearance::create();
		c.appearance->setFace(3);
		c.appearance->setHeight(1.25f);
		auto part = std::make_unique<PlayerAccountData>(*c.account, *c.commonData, *c.appearance);
		c.accountData = Ptr<PlayerAccountData>(*part);
		c.account->addPlayerAccountData(std::move(part));
		return c;
	}
};

TEST_F(PlayerServiceTest, NameUsedOrReserved) {
	insertPlayer(2001, "Taken", 1);
	execute("INSERT INTO old_names (player_id, old_name, new_name) VALUES (2001, 'Renamed', 'Taken')");
	EXPECT_TRUE(PlayerService::isNameUsedOrReserved(std::nullopt, "Taken", 7));
	EXPECT_FALSE(PlayerService::isNameUsedOrReserved(std::nullopt, "Free", 7));
	// the old name of a renamed character is reserved for other characters ...
	EXPECT_TRUE(PlayerService::isNameUsedOrReserved(std::nullopt, "Renamed", 7));
	// ... but not for the renamed character itself (new_name == oldName) and not without a reservation duration
	EXPECT_FALSE(PlayerService::isNameUsedOrReserved(std::optional<std::string_view>("Taken"), "Renamed", 7));
	EXPECT_FALSE(PlayerService::isNameUsedOrReserved(std::nullopt, "Renamed", 0));
	// the two-argument overload uses NameConfig.RESERVE_OLD_NAME_DAYS
	AtomicConfigScope days(configs::main::NameConfig::RESERVE_OLD_NAME_DAYS, 0);
	EXPECT_FALSE(PlayerService::isNameUsedOrReserved(std::nullopt, "Renamed"));
	configs::main::NameConfig::RESERVE_OLD_NAME_DAYS = 3;
	EXPECT_TRUE(PlayerService::isNameUsedOrReserved(std::nullopt, "Renamed"));
}

TEST_F(PlayerServiceTest, DeletePlayerAndCancelDeletionStoreTheDeletionTime) {
	insertPlayer(2011, "Leaving", 2);
	NewCharacter c = newCharacter(2011, "Leaving", model::Race::ELYOS, model::PlayerClass::WARRIOR);
	AtomicConfigScope minutes(configs::main::CustomConfig::CHARACTER_DELETION_TIME_MINUTES, 5);

	const int64_t before = commons::utils::currentTimeMillis();
	PlayerService::deletePlayer(*c.accountData);
	ASSERT_TRUE(c.accountData->getDeletionDate());
	const int64_t deletion = c.accountData->getDeletionDate()->time_since_epoch().count();
	EXPECT_GE(deletion, before + 5 * 60 * 1000);
	EXPECT_LE(deletion, commons::utils::currentTimeMillis() + 5 * 60 * 1000);
	EXPECT_TRUE(queryString("SELECT deletion_date FROM players WHERE id = 2011"));

	// a second request keeps the first deletion date
	PlayerService::deletePlayer(*c.accountData);
	EXPECT_EQ(c.accountData->getDeletionDate()->time_since_epoch().count(), deletion);

	// the deletion time has not passed: cancelled, the column is NULL again
	EXPECT_TRUE(PlayerService::cancelPlayerDeletion(*c.accountData));
	EXPECT_FALSE(c.accountData->getDeletionDate());
	EXPECT_FALSE(queryString("SELECT deletion_date FROM players WHERE id = 2011"));
	// nothing to cancel
	EXPECT_TRUE(PlayerService::cancelPlayerDeletion(*c.accountData));

	// an expired deletion cannot be cancelled
	c.accountData->setDeletionDate(commons::database::Timestamp(std::chrono::milliseconds(before - 1000)));
	EXPECT_FALSE(PlayerService::cancelPlayerDeletion(*c.accountData));
	EXPECT_TRUE(c.accountData->getDeletionDate());
}

TEST_F(PlayerServiceTest, CreationTimeAndPlayerName) {
	insertPlayer(2021, "Named", 3);
	PlayerService::storeCreationTime(2021, commons::database::Timestamp(std::chrono::milliseconds(1767326645000))); // 2026-01-02 04:04:05 UTC
	EXPECT_TRUE(queryString("SELECT creation_date FROM players WHERE id = 2021"));
	// not in the world: read from the database; unknown ids give null
	EXPECT_EQ(PlayerService::getPlayerName(2021), std::optional<std::string>("Named"));
	EXPECT_EQ(PlayerService::getPlayerName(2099), std::nullopt);
}

TEST_F(PlayerServiceTest, DeletePlayerFromDbRemovesTheCharacterAndItsItems) {
	insertPlayer(2031, "Gone", 4);
	// notifyServices=false skips HousingService and BrokerService
	PlayerService::deletePlayerFromDB(2031, false);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM players WHERE id = 2031"), 0);
}

TEST_F(PlayerServiceTest, NewPlayerUsesTheSpawnLocationAndStoreNewPlayerWritesTheRows) {
	NewCharacter c = newCharacter(2041, "Newbie", model::Race::ELYOS, model::PlayerClass::WARRIOR);
	Ref<Player> player;
	SLICE_SKIP_IF_UNPORTED(player = PlayerService::newPlayer(*c.accountData, *c.account));
	// the Elyos spawn location of player_initial_data.xml
	EXPECT_EQ(c.commonData->getMapId(), 210010000);
	EXPECT_NEAR(c.commonData->getX(), 1212.9423f, 0.001f);
	EXPECT_NEAR(c.commonData->getY(), 1044.8516f, 0.001f);
	EXPECT_NEAR(c.commonData->getZ(), 140.75568f, 0.001f);
	EXPECT_EQ(c.commonData->getHeading(), 32);
	EXPECT_TRUE(player->getMailbox());
	EXPECT_EQ(player->getInventory().getPersistentState(), model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);

	bool stored = false;
	SLICE_SKIP_IF_UNPORTED(stored = PlayerService::storeNewPlayer(*player, c.account->getName(), c.account->getId()));
	EXPECT_TRUE(stored);
	EXPECT_EQ(queryString("SELECT name FROM players WHERE id = 2041"), std::optional<std::string>("Newbie"));
	EXPECT_EQ(queryLong("SELECT world_id FROM players WHERE id = 2041"), 210010000);
	EXPECT_EQ(queryLong("SELECT account_id FROM players WHERE id = 2041"), 21);
	EXPECT_EQ(queryLong("SELECT online FROM players WHERE id = 2041"), 0);
	EXPECT_EQ(queryLong("SELECT face FROM player_appearance WHERE player_id = 2041"), 3);

	// a second character with the same name: the players insert fails and storeNewPlayer returns false
	NewCharacter twin = newCharacter(2042, "Newbie", model::Race::ELYOS, model::PlayerClass::WARRIOR);
	Ref<Player> twinPlayer = PlayerService::newPlayer(*twin.accountData, *twin.account);
	EXPECT_FALSE(PlayerService::storeNewPlayer(*twinPlayer, twin.account->getName(), twin.account->getId()));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM players WHERE name = 'Newbie'"), 1);
}

TEST_F(PlayerServiceTest, StorePlayerWritesThePosition) {
	NewCharacter c = newCharacter(2051, "Walker", model::Race::ASMODIANS, model::PlayerClass::MAGE);
	Ref<Player> player;
	SLICE_SKIP_IF_UNPORTED(player = PlayerService::newPlayer(*c.accountData, *c.account));
	EXPECT_EQ(c.commonData->getMapId(), 220010000);
	bool stored = false;
	SLICE_SKIP_IF_UNPORTED(stored = PlayerService::storeNewPlayer(*player, c.account->getName(), c.account->getId()));
	ASSERT_TRUE(stored);

	// like a relogin: the account and the player come from the database (getPlayer loads the settings, lists and storages storePlayer writes)
	Ref<Account> account;
	SLICE_SKIP_IF_UNPORTED(account = services::AccountService::loadAccount(21));
	Ref<Player> loaded;
	SLICE_SKIP_IF_UNPORTED(loaded = PlayerService::getPlayer(2051, account));
	loaded->setPosition(world::WorldPosition::create(220010000, 600.5f, 2800.25f, 300.0f, 12));
	SLICE_SKIP_IF_UNPORTED(PlayerService::storePlayer(*loaded));
	// the loaded player is not logged out: its account warehouse still retains it
	model::gameobjects::player::LogoutBreakers::run(*loaded);
	EXPECT_EQ(queryLong("SELECT world_id FROM players WHERE id = 2051"), 220010000);
	EXPECT_EQ(queryLong("SELECT heading FROM players WHERE id = 2051"), 12);
	EXPECT_EQ(queryLong("SELECT CAST(ROUND(x * 100) AS SIGNED) FROM players WHERE id = 2051"), 60050);
	EXPECT_EQ(queryLong("SELECT CAST(ROUND(y * 100) AS SIGNED) FROM players WHERE id = 2051"), 280025);
}

} // namespace
} // namespace aion::gameserver::loginslice::test
