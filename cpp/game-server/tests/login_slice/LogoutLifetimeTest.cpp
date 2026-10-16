// C++ lifetime additions of the login slice (P5-00, m5a-plan.md S-05/S-09/S-10; runtime-architecture.md §5.3 RR-5): the logout breakers run when
// PlayerService.getPlayer fails after the account warehouse took the player as its actor, and when leaveWorld throws; a player dropped this way,
// or logged out during a pending item use, is destroyed (SlicePlayer counts its destructor). The players come from the test database through
// AccountService.loadAccount and PlayerService.getPlayer, like at enter world.
// Cases that need bodies of other chunks that are still AION_UNPORTED skip themselves.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "SliceDbTest.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dao/PlayerAppearanceDAO.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/services/AccountService.h"
#include "aion/gameserver/services/player/PlayerLeaveWorldService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::loginslice::test {
namespace {

using model::account::Account;
using model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

/** Runs the statement; an unported body of another chunk skips the test, any other exception is kept in `thrown` */
#define LIFETIME_RUN(statement, thrown)                                                                                                        \
	try {                                                                                                                                      \
		statement;                                                                                                                             \
	} catch (const ::aion::gameserver::runtime::UnportedException& unported) {                                                                  \
		GTEST_SKIP() << "another chunk is not ported yet: " << unported.what();                                                               \
	} catch (const std::exception& e) {                                                                                                        \
		thrown = e.what();                                                                                                                     \
	}

class LogoutLifetimeTest : public SliceDbTest {
protected:
	/** stores a character (players and player_appearance rows) */
	static void storeCharacter(int32_t objectId, std::string_view name, int32_t accountId) {
		insertPlayer(objectId, name, accountId);
		Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
		ASSERT_TRUE(dao::PlayerAppearanceDAO::store(objectId, *appearance));
	}

	static Ptr<Player> warehouseActor(Account& account) {
		return static_cast<model::items::storage::PlayerStorage&>(account.getAccountWarehouse()).getActor();
	}
};

TEST_F(LogoutLifetimeTest, GetPlayerLoadFailureAfterTheWarehouseOwnerRunsTheBreakers) {
	storeCharacter(3001, "Loader", 31);
	Ref<Account> account;
	SLICE_SKIP_IF_UNPORTED(account = services::AccountService::loadAccount(31));
	ASSERT_TRUE(account->getPlayerAccountData(3001));

	static std::atomic<bool> hookRan{false};
	hookRan = false;
	services::player::PlayerService::setLoadHookForTests([](Player& player) {
		hookRan = true;
		// the warehouse took the player as its actor right before the hook
		EXPECT_TRUE(warehouseActor(*player.getAccount()) == Ptr<Player>(player));
		throw commons::utils::IllegalStateException("injected load failure");
	});
	std::string thrown;
	LIFETIME_RUN(services::player::PlayerService::getPlayer(3001, account), thrown);
	ASSERT_TRUE(hookRan.load()) << thrown;
	EXPECT_NE(thrown.find("injected load failure"), std::string::npos) << thrown;
	EXPECT_FALSE(warehouseActor(*account)) << "L3: the account warehouse no longer retains the dropped player";

	// the dropped player is destroyed although the account is still held
	closeScopeAndDrain();
	EXPECT_EQ(createdSlicePlayers.load(), 1);
	EXPECT_EQ(destroyedSlicePlayers.load(), 1);
	reopenScope();
	EXPECT_EQ(account->size(), 1);
}

TEST_F(LogoutLifetimeTest, LeaveWorldRunsTheBreakersWhenItThrows) {
	storeCharacter(3011, "Leaver", 32);
	storeCharacter(3012, "Target", 33);
	Ref<Account> account;
	{
		Ref<Player> player;
		Ref<Player> target;
		std::string thrown;
		LIFETIME_RUN(account = services::AccountService::loadAccount(32), thrown);
		LIFETIME_RUN(player = services::player::PlayerService::getPlayer(3011, account), thrown);
		Ref<Account> targetAccount;
		LIFETIME_RUN(targetAccount = services::AccountService::loadAccount(33), thrown);
		LIFETIME_RUN(target = services::player::PlayerService::getPlayer(3012, targetAccount), thrown);
		ASSERT_TRUE(player && target) << thrown;
		player->setTarget(target);
		ASSERT_TRUE(player->getTarget());
		ASSERT_TRUE(warehouseActor(*account) == Ptr<Player>(player));

		// the player never entered the world and has no connection: leaveWorld cannot complete (Java: NullPointerException at the latest on
		// con.setActivePlayer(null)), and every exception on the way must still run the breakers
		thrown.clear();
		LIFETIME_RUN(services::player::PlayerLeaveWorldService::leaveWorld(*player), thrown);
		EXPECT_FALSE(thrown.empty()) << "leaveWorld without a connection throws";
		EXPECT_FALSE(player->getTarget()) << "L1";
		EXPECT_FALSE(warehouseActor(*account)) << "L3";
		EXPECT_FALSE(player->getClientConnection());
		// the target stays "logged in": its own logout breakers release it (its account warehouse retains it)
		model::gameobjects::player::LogoutBreakers::run(*target);
	}
	closeScopeAndDrain();
	EXPECT_EQ(createdSlicePlayers.load(), 2);
	EXPECT_EQ(destroyedSlicePlayers.load(), 2);
	reopenScope();
}

TEST_F(LogoutLifetimeTest, LogoutDuringItemUseCancelsTheTaskAndReleasesThePlayer) {
	storeCharacter(3021, "User", 34);
	Ref<Account> account;
	runtime::FutureRef itemUse;
	{
		Ref<Player> player;
		std::string thrown;
		LIFETIME_RUN(account = services::AccountService::loadAccount(34), thrown);
		LIFETIME_RUN(player = services::player::PlayerService::getPlayer(3021, account), thrown);
		ASSERT_TRUE(player) << thrown;
		// entered the world (PlayerEnterWorldService.enterWorld stores it; World.removeObject deletes only stored objects)
		world::World::getInstance().storeObject(*player);
		// Java ItemUseAction: a 5 s task pinned on the player, kept as the controller's ITEM_USE task
		itemUse = utils::ThreadPoolManager::getInstance().schedule(runtime::Pin(player), [] {}, 5000);
		player->getController().addTask(model::TaskId::ITEM_USE, itemUse);
		// without a client connection leaveWorld ends with Java's NullPointerException, after the controller deleted the player
		LIFETIME_RUN(services::player::PlayerLeaveWorldService::leaveWorld(*player), thrown);
		EXPECT_NE(thrown.find("\"con\" is null"), std::string::npos) << "the null connection at the end of leaveWorld: " << thrown;
		EXPECT_TRUE(itemUse->isCancelled()) << "controller.delete() cancels the tasks of the player";
	}
	itemUse = nullptr;
	closeScopeAndDrain();
	EXPECT_EQ(createdSlicePlayers.load(), 1);
	EXPECT_EQ(destroyedSlicePlayers.load(), 1);
	reopenScope();
}

} // namespace
} // namespace aion::gameserver::loginslice::test
