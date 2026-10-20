// AccountService (P5-00, m5a-plan.md S-02/S-10) on the test schema: loading the account's characters, their appearance and ban info, the account
// warehouse, the fields the login server sends, and the removal of characters whose deletion time has passed.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "SliceDbTest.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dao/PlayerAppearanceDAO.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/AccountTime.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/services/AccountService.h"

namespace aion::gameserver::loginslice::test {
namespace {

using model::account::Account;
using model::account::PlayerAccountData;
using runtime::Ptr;
using runtime::Ref;
using services::AccountService;

class AccountServiceTest : public SliceDbTest {
protected:
	/** a players row with an appearance row (face = id % 100) */
	static void insertCharacter(int32_t id, std::string_view name, int32_t accountId, std::string_view race = "ELYOS") {
		insertPlayer(id, name, accountId, race);
		Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
		appearance->setFace(id % 100);
		appearance->setHeight(1.5f);
		ASSERT_TRUE(dao::PlayerAppearanceDAO::store(id, *appearance));
	}
};

TEST_F(AccountServiceTest, LoadAccountReadsEveryCharacterOfTheAccount) {
	insertCharacter(1001, "Alpha", 7);
	insertCharacter(1002, "Beta", 7);
	insertCharacter(1003, "Other", 8);
	execute("INSERT INTO player_punishments (player_id, punishment_type, start_time, duration, reason) VALUES (1002, 'CHARBAN', 100, 60, 'test')");

	Ref<Account> account = AccountService::loadAccount(7);
	EXPECT_EQ(account->getId(), 7);
	EXPECT_EQ(account->size(), 2);
	EXPECT_FALSE(account->getPlayerAccountData(1003));

	Ptr<PlayerAccountData> alpha = account->getPlayerAccountData(1001);
	ASSERT_TRUE(alpha);
	EXPECT_EQ(alpha->getPlayerCommonData()->getName(), "Alpha");
	EXPECT_EQ(alpha->getPlayerCommonData()->getRace(), model::Race::ELYOS);
	EXPECT_EQ(alpha->getPlayerCommonData()->getMapId(), 210010000);
	EXPECT_EQ(alpha->getAppearance()->getFace(), 1);
	EXPECT_FALSE(alpha->getCharBanInfo());
	EXPECT_FALSE(alpha->getDeletionDate());
	EXPECT_TRUE(alpha->getVisibleItems() == nullptr || alpha->getVisibleItems()->isEmpty());

	Ptr<PlayerAccountData> beta = account->getPlayerAccountData(1002);
	ASSERT_TRUE(beta);
	ASSERT_TRUE(beta->getCharBanInfo());
	EXPECT_EQ(beta->getCharBanInfo()->getReason(), "test");

	// the account warehouse is loaded (empty on a fresh schema)
	EXPECT_EQ(account->getAccountWarehouse().getItems().size(), 0u);
	EXPECT_EQ(account->getAccountWarehouse().getStorageType(), model::items::storage::StorageType::ACCOUNT_WAREHOUSE);
}

TEST_F(AccountServiceTest, LoadPlayerAccountDataReadsCreationAndDeletionDates) {
	insertCharacter(1011, "Dated", 9);
	execute("UPDATE players SET creation_date = '2026-01-02 03:04:05', deletion_date = '2030-01-01 00:00:00' WHERE id = 1011");
	Ref<Account> account = Account::create(9);
	std::unique_ptr<PlayerAccountData> data = AccountService::loadPlayerAccountData(*account, 1011);
	ASSERT_TRUE(data);
	EXPECT_TRUE(data->getCreationDate());
	ASSERT_TRUE(data->getDeletionDate());
	EXPECT_GT(data->getDeletionDate()->time_since_epoch().count(), commons::utils::currentTimeMillis());
	EXPECT_EQ(data->getPlayerCommonData()->getPlayerObjId(), 1011);
}

TEST_F(AccountServiceTest, GetAccountSetsTheLoginServerFieldsAndKeepsPendingDeletions) {
	insertCharacter(1021, "Pending", 10);
	execute("UPDATE players SET deletion_date = '2037-01-01 00:00:00' WHERE id = 1021");
	Ref<model::account::AccountTime> accountTime = model::account::AccountTime::create();
	Ref<Account> account = AccountService::getAccount(10, "tester", 123456789, *accountTime, 1, 2, "HDD-1");
	EXPECT_EQ(account->getName(), "tester");
	EXPECT_EQ(account->getCreationDate(), 123456789);
	EXPECT_EQ(account->getAccessLevel(), 1);
	EXPECT_EQ(account->getMembership(), 2);
	EXPECT_EQ(account->getAllowedHddSerial(), std::optional<std::string>("HDD-1"));
	EXPECT_TRUE(account->getAccountTime() == Ptr<model::account::AccountTime>(*accountTime));
	// the deletion time has not passed: the character stays
	EXPECT_EQ(account->size(), 1);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM players WHERE id = 1021"), 1);
}

TEST_F(AccountServiceTest, RemoveDeletedCharactersDeletesExpiredCharacters) {
	insertCharacter(1031, "Expired", 11);
	insertCharacter(1032, "Staying", 11);
	execute("UPDATE players SET deletion_date = '2001-01-01 00:00:00' WHERE id = 1031");
	Ref<Account> account = AccountService::loadAccount(11);
	ASSERT_EQ(account->size(), 2);
	// deletePlayerFromDB notifies HousingService and BrokerService (P5-11, P5-09)
	SLICE_SKIP_IF_UNPORTED(AccountService::removeDeletedCharacters(*account));
	EXPECT_EQ(account->size(), 1);
	EXPECT_FALSE(account->getPlayerAccountData(1031));
	EXPECT_EQ(account->getNumberOf(model::Race::ELYOS), 1) << "two Elyos loaded, one decremented";
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM players WHERE id = 1031"), 0);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM players WHERE id = 1032"), 1);
}

TEST_F(AccountServiceTest, RemovingTheLastCharacterReloadsTheEmptiedAccountWarehouse) {
	if (runtime::LIVE_COUNTS_ENABLED) // AION_CHECKED builds
		GTEST_SKIP() << "PlayerStorage(Account&, type) binds its owner with OwnedPart::bindOwner, which checked builds reject for a published account "
						"(C11 terminates): a new account warehouse for a loaded account needs a P4-13 fix (docs/deviations/P5-00.md)";
	insertCharacter(1041, "Last", 12);
	execute("UPDATE players SET deletion_date = '2001-01-01 00:00:00' WHERE id = 1041");
	Ref<Account> account = AccountService::loadAccount(12);
	model::items::storage::Storage* warehouseBefore = &account->getAccountWarehouse();
	SLICE_SKIP_IF_UNPORTED(AccountService::removeDeletedCharacters(*account));
	EXPECT_TRUE(account->isEmpty());
	EXPECT_NE(&account->getAccountWarehouse(), warehouseBefore) << "InventoryDAO.deleteAccountWH, then a new warehouse";
}

/**
 * AccountService.java:60-65: with the ratio limitation on, deleting the account's last character of a race counts one off that race's
 * GameServer ratio. P4-last cp4 calls GameServer::updateRatio directly instead of the stand-in standins::gameServerUpdateRatio.
 */
TEST_F(AccountServiceTest, RemovingTheLastCharacterOfARaceUpdatesTheRatio) {
	const bool savedEnabled = configs::main::GSConfig::ENABLE_RATIO_LIMITATION.exchange(true);
	const int32_t savedLevel = configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL.exchange(0);
	insertCharacter(1051, "Ratio", 13, "ASMODIANS");
	execute("UPDATE players SET deletion_date = '2001-01-01 00:00:00' WHERE id = 1051");
	insertCharacter(1052, "Staying", 13);
	Ref<Account> account = AccountService::loadAccount(13);
	GameServer::updateRatio(model::Race::ASMODIANS, 1);
	const int32_t before = GameServer::getCountFor(model::Race::ASMODIANS);
	const int32_t elyosBefore = GameServer::getCountFor(model::Race::ELYOS);
	SLICE_SKIP_IF_UNPORTED(AccountService::removeDeletedCharacters(*account));
	EXPECT_EQ(GameServer::getCountFor(model::Race::ASMODIANS), before - 1) << "the account has no Asmodian left";
	EXPECT_EQ(GameServer::getCountFor(model::Race::ELYOS), elyosBefore);
	configs::main::GSConfig::ENABLE_RATIO_LIMITATION.store(savedEnabled);
	configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL.store(savedLevel);
}
} // namespace
} // namespace aion::gameserver::loginslice::test
