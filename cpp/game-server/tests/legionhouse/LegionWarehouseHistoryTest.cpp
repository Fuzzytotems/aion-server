// P5-11 LegionService.addWHItemHistory (m5b3-plan.md T-08, under the I-03 lease of the items lane): the history of an item moved into or out
// of the legion warehouse (LegionService.java:1082-1092). Every cross-storage CM_SPLIT_ITEM reaches it (ItemSplitService.java:80, :94) and
// CM_MOVE_ITEM does for the legion warehouse (ItemMoveService.java:56-58), legion or not (m5b3-plan.md §2.8 E-9).
//
// Only the arm without a legion is testable: Legion's constructor is still AION_UNPORTED (Legion.cpp, P5-11), so no test can give the player
// a legion, and a legion member's arm reaches the unported addHistory (M5h). No database: LegionService's constructor reads none.

#include <gtest/gtest.h>

#include <memory>

#include "LegionHouseTestSupport.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/services/LegionService.h"

namespace aion::gameserver::legionhouse::test {
namespace {

using model::items::storage::PlayerStorage;
using model::items::storage::StorageType;

class LegionWarehouseHistoryTest : public LegionHouseTest {};

TEST_F(LegionWarehouseHistoryTest, AnItemMovedByAPlayerWithoutALegionWritesNoHistory) {
	// LegionService.java:1083-1084: `Legion legion = player.getLegion(); if (legion != null) ...` - nothing else happens without a legion,
	// whichever storage is the legion warehouse
	PlayerFixture fixture = makePlayer(100101, 1101);
	ASSERT_FALSE(fixture.player->getLegion());
	// a storage of type LEGION_WAREHOUSE without a legion (LegionWarehouse needs a Legion): only its getStorageType() is read
	std::unique_ptr<PlayerStorage> legionTypedPart = std::make_unique<PlayerStorage>(*fixture.player, StorageType::LEGION_WAREHOUSE);
	PlayerStorage& legionTyped = *legionTypedPart;
	services::LegionService& service = services::LegionService::getInstance();

	EXPECT_NO_THROW(service.addWHItemHistory(*fixture.player, 162000002, 5, legionTyped, fixture.player->getInventory())) << "a withdrawal";
	EXPECT_NO_THROW(service.addWHItemHistory(*fixture.player, 162000002, 5, fixture.player->getInventory(), legionTyped)) << "a deposit";
	EXPECT_NO_THROW(service.addWHItemHistory(*fixture.player, 162000002, 5, fixture.player->getInventory(), fixture.player->getWarehouse()))
		<< "the cube to the regular warehouse, CM_SPLIT_ITEM's case";
}

} // namespace
} // namespace aion::gameserver::legionhouse::test
