// S0c declaration headers of the item model (docs/design/hub-headers.md §3.5): the StorageType companion, the ported layout constructors and the
// runtime bases of the item stones.

#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <optional>

#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/model/items/ItemStone.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::model::items {
namespace {

// ItemStone has no runtime base: GodStone and ManaStone are RefCounted, IdianStone is a part of its item (cycles review); ChargeInfo is an
// ActionObserver (Java extends ActionObserver).
static_assert(std::derived_from<GodStone, runtime::RefCounted> && std::derived_from<ManaStone, runtime::RefCounted>);
static_assert(std::derived_from<IdianStone, runtime::OwnedPart> && std::derived_from<IdianStone, ItemStone>);
static_assert(!std::derived_from<ItemStone, runtime::RefCounted> && !std::derived_from<ItemStone, runtime::OwnedPart>);
static_assert(std::derived_from<ChargeInfo, controllers::observer::ActionObserver>);
static_assert(runtime::Retainable<GodStone> && runtime::Retainable<ManaStone>);

// StorageType companion: Java constructor data and lookups
static_assert(storage::getLimit(storage::StorageType::CUBE) == 27 && storage::getSpecialLimit(storage::StorageType::CUBE) == 102);
static_assert(storage::getId(storage::StorageType::LEGION_WAREHOUSE) == 3 && storage::getLength(storage::StorageType::REGULAR_WAREHOUSE) == 8);
static_assert(storage::getStorageTypeById(storage::PET_BAG_MIN) == storage::StorageType::PET_BAG_6);
static_assert(storage::getStorageTypeById(storage::HOUSE_WH_MAX) == storage::StorageType::HOUSE_STORAGE_20);
static_assert(storage::getStorageTypeById(5) == std::nullopt && storage::getStorageId(56, 8) == 3 && storage::getStorageId(1, 1) == -1);
static_assert(storage::PET_BAG_MAX - storage::PET_BAG_MIN + 1 == 12 && storage::HOUSE_WH_MAX - storage::HOUSE_WH_MIN + 1 == 20);

TEST(ItemsModelDeclarationsTest, ItemStorageTakesTheLimitOfItsStorageType) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<storage::ItemStorage> inventory = storage::ItemStorage::create(storage::StorageType::CUBE);
	EXPECT_EQ(inventory->getLimit(), 27);
	inventory->setLimit(36);
	EXPECT_EQ(inventory->getLimit(), 36);
	EXPECT_EQ(storage::ItemStorage::FIRST_AVAILABLE_SLOT, 65535);
	EXPECT_EQ(storage::ItemStorage::create(storage::StorageType::BROKER)->getLimit(), 0);
}

TEST(ItemsModelDeclarationsTest, ValueObjectsStoreTheirConstructorArguments) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<ItemCooldown> cooldown = ItemCooldown::create(123456, 30);
	EXPECT_EQ(cooldown->getReuseTime(), 123456);
	EXPECT_EQ(cooldown->getUseDelay(), 30);
	runtime::Ref<PendingTuneResult> tune = PendingTuneResult::create(1, 2, 3, true);
	EXPECT_EQ(tune->getOptionalSockets(), 1);
	EXPECT_EQ(tune->getStatBonusId(), 3);
	EXPECT_TRUE(tune->isAttributeOnly());
}

} // namespace
} // namespace aion::gameserver::model::items
