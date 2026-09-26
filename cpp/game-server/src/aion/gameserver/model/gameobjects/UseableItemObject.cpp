#include "aion/gameserver/model/gameobjects/UseableItemObject.h"

#include <chrono>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/housing/HousingUseableItem.h"
#include "aion/gameserver/model/templates/housing/LimitType.h"
#include "aion/gameserver/model/templates/housing/UseItemAction.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_OBJECT_USE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::gameobjects {

/** Java: LoggerFactory.getLogger(UseableItemObject.class) inside onUse */
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.UseableItemObject");

namespace {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/** Java `DataManager.ITEM_DATA.getItemTemplate(itemId)` (NullPointerException while the item data is not published) */
const templates::item::ItemTemplate* findItemTemplate(int32_t itemId) {
	return dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
}

/** Java `PlaceableHouseObject.getPlacementLimit()`: the shell declares no accessor for the `limit` attribute yet (P4-07b) */
templates::housing::LimitType placementLimitOf(const templates::housing::HousingUseableItem* template_) {
	static_cast<void>(template_);
	AION_UNPORTED();
}

/** Java auto-unboxing of a null Integer: NullPointerException */
int32_t unbox(const std::optional<int32_t>& value) {
	if (!value)
		throw runtime::NullPointerException("null Integer");
	return *value;
}

} // namespace

UseableItemObject::UseDataWriter::UseDataWriter(UseableItemObject& value) : obj(value) {
}

UseableItemObject::UseDataWriter::~UseDataWriter() = default;

runtime::Ref<UseableItemObject::UseDataWriter> UseableItemObject::UseDataWriter::create(UseableItemObject& value) {
	return runtime::makeRef<UseDataWriter>(value);
}

void UseableItemObject::UseDataWriter::writeMe(commons::utils::ByteBuffer& buffer) {
	writeD(buffer, !obj.getObjectTemplate()->getUseCount() ? 0 : obj.getOwnerUsedCount() + obj.getVisitorUsedCount());
	const templates::housing::UseItemAction* action = obj.getObjectTemplate()->getAction();
	writeC(buffer, action == nullptr || !action->getCheckType() ? 0 : *action->getCheckType());
}

UseableItemObject::UseableItemObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: UseableHouseObject(key, registry, objId, templateId), entryWriter(UseDataWriter::create(*this)) {
	// Java order: mustGiveLastReward, then entryWriter = new UseDataWriter(this) (the writer only stores the object)
	const templates::housing::UseItemAction* action = getObjectTemplate()->getAction();
	if (action != nullptr && action->getFinalRewardId() && isExpired())
		mustGiveLastReward.set(true);
}

UseableItemObject::~UseableItemObject() = default;

const templates::housing::HousingUseableItem* UseableItemObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingUseableItem*>(HouseObject::getObjectTemplate());
}

void UseableItemObject::onUse(player::Player& player) {
	const templates::housing::UseItemAction* action = getObjectTemplate()->getAction();
	if (action == nullptr) { // Some objects do not have actions; they are test items now
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_ALL_CANT_USE());
		return;
	}

	int32_t ownerId = getOwnerHouse()->getOwnerId();
	bool isOwner = ownerId == player.getObjectId();
	if (!isOwner && getObjectTemplate()->isOwnerOnly()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_IS_ONLY_FOR_OWNER_VALID());
		return;
	}

	if (player.getHouseObjectCooldowns()->hasCooldown(getObjectId())) {
		if (getObjectTemplate()->getCd() && *getObjectTemplate()->getCd() > 0)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANNOT_USE_FLOWERPOT_COOLTIME());
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_CANT_USE_PER_DAY());
		return;
	}

	const std::optional<int32_t> useCount = getObjectTemplate()->getUseCount();
	int32_t currentUseCount = 0;
	if (useCount) {
		// Counter is for both, but could be made custom from configs
		currentUseCount = getOwnerUsedCount() + getVisitorUsedCount();
		if (currentUseCount >= *useCount && !isOwner || currentUseCount > *useCount && isOwner) {
			// if expiration is set then final reward has to be given for owner only due to inventory full. If inventory was not full, the object had to
			// be despawned, so we wouldn't reach this check.
			if (!mustGiveLastReward.get() || !isOwner) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_ACHIEVE_USE_COUNT());
				return;
			}
		}
	}

	if (mustGiveLastReward.get() && !isOwner) { // expired, wait for owner
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_DELETE_EXPIRE_TIME(getObjectTemplate()->getL10n()));
		return;
	}

	if (placementLimitOf(getObjectTemplate()) == templates::housing::LimitType::COOKING) {
		// Check if player already has an item
		if (player.getInventory().getItemCountByItemId(unbox(action->getRewardId())) > 0) {
			std::string rewardL10n = findItemTemplate(unbox(action->getRewardId()))->getL10n();
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_USE_ALREADY_HAVE_REWARD_ITEM(rewardL10n, getObjectTemplate()->getL10n()));
			return;
		}
	}

	const std::optional<int32_t> requiredItem = getObjectTemplate()->getRequiredItem();
	if (requiredItem) {
		if (unbox(action->getCheckType()) == 1) { // equip item needed
			if (player.getEquipment().getEquippedItemsByItemId(*requiredItem).size() == 0) {
				std::string requiredItemL10n = findItemTemplate(*requiredItem)->getL10n();
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_USE_HOUSE_OBJECT_ITEM_EQUIP(requiredItemL10n));
				return;
			}
		} else if (player.getInventory().getItemCountByItemId(*requiredItem) < unbox(action->getRemoveCount())) {
			std::string requiredItemL10n = findItemTemplate(*requiredItem)->getL10n();
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_USE_HOUSE_OBJECT_ITEM_CHECK(requiredItemL10n));
			return;
		}
	}

	if (requiredItem.has_value() ^ action->getRemoveCount().has_value()) {
		log.warn(toString() + " doesn't have valid usage requirements " + (!requiredItem ? " (item missing)" : "(remove count missing)"));
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_ALL_CANT_USE());
		return;
	}

	if (player.getInventory().isFull()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_WAREHOUSE_TOO_MANY_ITEMS_INVENTORY());
		return;
	}

	if (!setOccupant(player)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_OCCUPIED_BY_OTHER());
		return;
	}

	const int32_t delayMs = getObjectTemplate()->getDelay();
	const int32_t usedCount = !useCount ? 0 : currentUseCount + 1;
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_USE(getObjectTemplate()->getL10n()));
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_USE_OBJECT(player.getObjectId(), getObjectId(), delayMs, 8));
	player.getController().addTask(TaskId::HOUSE_OBJECT_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player},
			[this, &player, requiredItem, useCount, usedCount, isOwner, ownerId] {
				// Java captures `action`; it is the immutable template's action, read again here (lint L5 accepts no template pointer capture)
				const templates::housing::UseItemAction* action = getObjectTemplate()->getAction();
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_USE_OBJECT(player.getObjectId(), getObjectId(), 0, 9));
				if (requiredItem && action->getRemoveCount() && *action->getRemoveCount() > 0) {
					if (!player.getInventory().decreaseByItemId(*requiredItem, *action->getRemoveCount()))
						return;
				}

				int32_t rewardId = 0;
				bool delete_ = false;

				if (useCount) {
					if (action->getFinalRewardId() && *useCount + 1 == usedCount) {
						// visitors do not get final rewards
						rewardId = *action->getFinalRewardId();
						delete_ = true;
					} else if (action->getRewardId()) {
						rewardId = *action->getRewardId();
						if (*useCount == usedCount) {
							PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_FLOWERPOT_GOAL(getObjectTemplate()->getL10n()));
							if (!action->getFinalRewardId()) {
								delete_ = true;
							} else {
								setMustGiveLastReward(true);
								setExpireTime(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000));
								setPersistentState(PersistentState::UPDATE_REQUIRED);
							}
						}
					}
				} else if (action->getRewardId()) {
					rewardId = *action->getRewardId();
				}
				if (usedCount > 0) {
					if (!delete_) {
						if (isOwner)
							incrementOwnerUsedCount();
						else
							incrementVisitorUsedCount();
					}
				}
				if (rewardId > 0) {
					services::item::ItemService::addItem(player, rewardId, 1);
					std::string rewardL10n = findItemTemplate(rewardId)->getL10n();
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_REWARD_ITEM(getObjectTemplate()->getL10n(), rewardL10n));
				}
				PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_OBJECT_USE_UPDATE(player.getObjectId(), ownerId, usedCount, *this),
					true);
				if (delete_) {
					despawnAndRemoveHouseObject(player, false);
				} else {
					int64_t reuseTime;
					std::optional<int32_t> cd = getObjectTemplate()->getCd();
					if (!cd || *cd == 0) { // use once per day (cooldown ends at midnight)
						const std::chrono::time_zone* zone = configs::main::GSConfig::TIME_ZONE_ID.load(); // Java: ServerTime.now()
						if (zone == nullptr)
							throw runtime::NullPointerException("GSConfig.TIME_ZONE_ID");
						reuseTime = detail::endOfServerDayMillis(commons::utils::currentTimeMillis(), zone);
					} else { // Java int arithmetic: cd * 1000 wraps before it is widened
						reuseTime = detail::cooldownReuseTimeMillis(commons::utils::currentTimeMillis(), *cd);
					}
					player.getHouseObjectCooldowns()->put(getObjectId(), reuseTime);
				}
			},
			delayMs));
}

bool UseableItemObject::canExpireNow() {
	return !mustGiveLastReward.get() && !isOccupied();
}

void UseableItemObject::writeUsageData(commons::utils::ByteBuffer& buffer) {
	entryWriter->writeMe(buffer);
}

bool UseableItemObject::hasUseCooldown() {
	return getObjectTemplate()->getCd() && *getObjectTemplate()->getCd() > 0;
}

} // namespace aion::gameserver::model::gameobjects
