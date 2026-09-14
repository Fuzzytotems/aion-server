// S0c declaration headers of the player model (docs/design/hub-headers.md §3.5): the shapes other chunks compile against and the constructors,
// trivial accessors and part bindings that are ported so the objects can be created.

#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/AccountTime.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/account/CharacterPasskey.h"
#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::model::gameobjects::player {
namespace {

// Parts of Player and Account derive OwnedPart; the retainable interfaces are forwarded by the implementors (hub-headers.md §9.2).
static_assert(std::derived_from<account::PlayerAccountData, runtime::OwnedPart>);
static_assert(std::derived_from<title::TitleList, runtime::OwnedPart> && std::derived_from<items::storage::PlayerStorage, items::storage::Storage>);
static_assert(runtime::Retainable<emotion::Emotion> && runtime::Retainable<motion::Motion> && runtime::Retainable<title::Title>);
static_assert(std::derived_from<Cooldowns, runtime::ConcurrentHashMap<int32_t, int64_t>>);

TEST(PlayerModelDeclarationsTest, AccountAndAccountWarehouseArePortedLayouts) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<account::Account> account = account::Account::create(42);
	EXPECT_EQ(account->getId(), 42);
	EXPECT_TRUE(account->equals(*account));
	EXPECT_EQ(account->hashCode(), 42);
	EXPECT_EQ(account->getSecurityToken(), "");
	EXPECT_EQ(account->getAllowedHddSerial(), std::nullopt);
	account->setAllowedHddSerial("serial");
	EXPECT_EQ(account->getAllowedHddSerial(), std::optional<std::string>("serial"));
	account->setAllowedHddSerial(std::nullopt);
	EXPECT_EQ(account->getAllowedHddSerial(), std::nullopt);
	EXPECT_EQ(account->getLastStamp(), std::nullopt);

	// Java: new PlayerStorage(null, StorageType.ACCOUNT_WAREHOUSE): bound to the account, no actor
	account->setAccountWarehouse(std::make_unique<items::storage::PlayerStorage>(*account, items::storage::StorageType::ACCOUNT_WAREHOUSE));
	EXPECT_EQ(account->getAccountWarehouse().getStorageType(), items::storage::StorageType::ACCOUNT_WAREHOUSE);

	account->setAccountTime(account::AccountTime::create());
	ASSERT_TRUE(account->getAccountTime());
	account->getAccountTime()->setAccumulatedOnlineTime(5);
	EXPECT_EQ(account->getAccountTime()->getAccumulatedOnlineTime(), 5);
}

TEST(PlayerModelDeclarationsTest, ValueObjectsStoreTheirConstructorArguments) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<account::CharacterBanInfo> cbi = account::CharacterBanInfo::create(1000, 500, "spam");
	EXPECT_EQ(cbi->getEnd(), 1500);
	EXPECT_EQ(cbi->getReason(), "spam");

	// Passport truncates the arrive date to seconds (normTs) and keeps null
	runtime::Ref<account::Passport> passport = account::Passport::create(3, false, commons::database::Timestamp(std::chrono::milliseconds(2999)));
	ASSERT_TRUE(passport->getArriveDate().has_value());
	EXPECT_EQ(passport->getArriveDate()->time_since_epoch(), std::chrono::milliseconds(2000));
	EXPECT_EQ(passport->getPersistentState(), Persistable::PersistentState::NOACTION);
	EXPECT_EQ(account::Passport::create(4, true, std::nullopt)->getArriveDate(), std::nullopt);

	runtime::Ref<BindPointPosition> bindPoint = BindPointPosition::create(110010000, 1.0f, 2.0f, 3.0f, int8_t{7});
	EXPECT_EQ(bindPoint->getMapId(), 110010000);
	EXPECT_EQ(bindPoint->getPersistentState(), Persistable::PersistentState::NEW);

	runtime::Ref<PlayerSettings> settings = PlayerSettings::create(nullptr, nullptr, nullptr, 1, 2);
	EXPECT_EQ(settings->getDeny(), 1);
	EXPECT_EQ(settings->getDisplay(), 2);
	EXPECT_FALSE(settings->getUiSettings());

	EXPECT_EQ(account::CharacterPasskey::create()->getWrongCount(), 0);
	EXPECT_EQ(PlayerAppearance::create()->getHeight(), 0.0f);
	EXPECT_EQ(QuestStateList::create()->getDeletedQuestIds().size(), 0);
	EXPECT_EQ(emotion::Emotion::create(5, 60)->getExpireTime(), 60);
	EXPECT_EQ(motion::Motion::motionType.at(26), 3);
	EXPECT_TRUE(motion::Motion::create(9, 0, true)->isActive());
	EXPECT_EQ(title::Title::create(nullptr, 12, 0)->getId(), 12);
}

TEST(PlayerModelDeclarationsTest, RecordsCompareAndHashLikeJava) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<Macros::Macro> macro = Macros::Macro::create(1, "ab");
	EXPECT_TRUE(macro->equals(*Macros::Macro::create(1, "ab")));
	EXPECT_FALSE(macro->equals(*Macros::Macro::create(2, "ab")));
	// Java: 31 * Integer.hashCode(1) + "ab".hashCode() = 31 + (31 * 'a' + 'b') = 31 + 3105
	EXPECT_EQ(macro->hashCode(), 31 + 3105);
	runtime::Ref<account::PlayerAccountData::VisibleItem> item = account::PlayerAccountData::VisibleItem::create(int8_t{1}, 2, 3, std::nullopt);
	EXPECT_TRUE(item->equals(*account::PlayerAccountData::VisibleItem::create(int8_t{1}, 2, 3, std::nullopt)));
	EXPECT_EQ(item->hashCode(), ((1 * 31 + 2) * 31 + 3) * 31 + 0);
}

TEST(PlayerModelDeclarationsTest, CollectionsAndSynchronizedAccessors) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<BlockedPlayer> blocked = BlockedPlayer::create(7, "Ben", "rude");
	blocked->setReason("very rude");
	EXPECT_EQ(blocked->getReason(), "very rude");
	runtime::Ref<BlockList> blockList = BlockList::create({{7, blocked}});
	EXPECT_EQ(BlockList::MAX_BLOCKS, 100);
	runtime::Ref<RecipeList> recipes = RecipeList::create({1, 2, 3});
	EXPECT_EQ(recipes->getRecipeList().size(), 3);
	runtime::Ref<Cooldowns> cooldowns = Cooldowns::create();
	EXPECT_THROW(cooldowns->hasCooldown(1), runtime::UnportedException);
}

TEST(PlayerModelDeclarationsTest, TitleListCreatedByTheDaoHasNoOwnerYet) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// A list created by the DAO without an owner is not bound until setOwner; the Java owner is null until then
	auto titleList = std::make_unique<title::TitleList>();
	EXPECT_FALSE(titleList->isOwnerBound());
	EXPECT_FALSE(titleList->getOwner());
}

} // namespace
} // namespace aion::gameserver::model::gameobjects::player
