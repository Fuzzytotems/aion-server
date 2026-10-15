#include "aion/gameserver/services/ArmsfusionService.h"

#include <optional>
#include <string>

#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/Improvement.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services {

namespace {

using model::gameobjects::Item;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/** Java: mainWeapon.getItemTemplate().getItemQuality() handed to a switch - a template without a quality is a NullPointerException */
model::templates::item::ItemQuality qualityOf(Item& item) {
	std::optional<model::templates::item::ItemQuality> quality = item.getItemTemplate()->getItemQuality();
	if (!quality)
		throw runtime::NullPointerException("ItemTemplate.getItemQuality()");
	return *quality;
}

} // namespace

// Java ArmsfusionService.java:22-97
void ArmsfusionService::fusionWeapons(model::gameobjects::player::Player& player, int32_t mainWeaponObjId, int32_t fuseWeaponObjId) {
	runtime::Ptr<Item> mainWeapon = player.getInventory().getItemByObjId(mainWeaponObjId);
	runtime::Ptr<Item> fuseWeapon = player.getInventory().getItemByObjId(fuseWeaponObjId);

	// Check if item is in bag
	if (mainWeapon == nullptr || fuseWeapon == nullptr) {
		if (player.getEquipment().getEquippedItemByObjId(mainWeaponObjId) != nullptr
			|| player.getEquipment().getEquippedItemByObjId(fuseWeaponObjId) != nullptr)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_EQUIPED_ITEM());
		else {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ITEM_NO_TARGET_ITEM());
			utils::audit::AuditLogger::log(player, "tried to fuse weapons he doesn't have (obj IDs:" + std::to_string(mainWeaponObjId) + ", "
				+ std::to_string(fuseWeaponObjId) + ")");
		}
		return;
	}

	if (!mainWeapon->getItemTemplate()->isCanFuse() || !fuseWeapon->getItemTemplate()->isCanFuse()) {
		Item& item = mainWeapon->getItemTemplate()->isCanFuse() ? *mainWeapon : *fuseWeapon;
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_NOT_AVAILABLE(item.getL10n()));
		utils::audit::AuditLogger::log(player, "tried to fuse item " + std::to_string(fuseWeapon->getItemId()) + " onto "
			+ std::to_string(mainWeapon->getItemId()) + " (" + std::to_string(item.getItemId()) + " isn't fusible)");
		return;
	}

	int64_t basePricePerLevelSquared = getBasePricePerLevelSquared(qualityOf(*mainWeapon));
	int32_t level = mainWeapon->getItemTemplate()->getLevel();
	// Java: basePricePerLevelSquared * level * level, long arithmetic
	int64_t basePrice = static_cast<int64_t>(static_cast<uint64_t>(basePricePerLevelSquared) * static_cast<uint64_t>(static_cast<int64_t>(level))
		* static_cast<uint64_t>(static_cast<int64_t>(level)));
	int64_t price = trade::PricesService::getPriceForService(basePrice, player.getRace());

	if (player.getInventory().getKinah() < price) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_NOT_ENOUGH_MONEY(mainWeapon->getL10n(), fuseWeapon->getL10n()));
		return;
	}

	if (mainWeapon->getTemporaryExchangeTime() != 0) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_TEMPORARY_EXCHANGE_ITEM());
		return;
	}

	// Fusioned weapons must be not fusioned
	if (mainWeapon->hasFusionedItem() || fuseWeapon->hasFusionedItem()) {
		Item& item = mainWeapon->hasFusionedItem() ? *mainWeapon : *fuseWeapon;
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_NOT_AVAILABLE(item.getL10n()));
		return;
	}

	// Fusioned weapons must have same type
	if (mainWeapon->getItemTemplate()->getItemGroup() != fuseWeapon->getItemTemplate()->getItemGroup()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_DIFFERENT_TYPE());
		return;
	}

	// Second weapon must have inferior or equal lvl. in relation to first weapon
	if (fuseWeapon->getItemTemplate()->getLevel() > mainWeapon->getItemTemplate()->getLevel()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_MAIN_REQUIRE_HIGHER_LEVEL());
		return;
	}

	// You can not combine Conditioning and Augmenting
	if (mainWeapon->getImprovement() != nullptr && fuseWeapon->getImprovement() != nullptr) {
		if (mainWeapon->getImprovement()->getChargeWay() != fuseWeapon->getImprovement()->getChargeWay()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_ERROR_NOT_COMPARABLE_ITEM());
			return;
		}
	}

	if (!player.getInventory().decreaseByObjectId(fuseWeaponObjId, 1))
		return;
	mainWeapon->setFusionedItem(fuseWeapon);
	item::ItemSocketService::copyFusionStones(*fuseWeapon, *mainWeapon);
	mainWeapon->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	dao::InventoryDAO::store(*mainWeapon, player);

	item::ItemPacketService::updateItemAfterInfoChange(player, *mainWeapon);
	player.getInventory().decreaseKinah(price);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_SUCCESS(mainWeapon->getL10n(), fuseWeapon->getL10n()));
}

// Java ArmsfusionService.java:99-116
int64_t ArmsfusionService::getBasePricePerLevelSquared(model::templates::item::ItemQuality rarity) {
	using model::templates::item::ItemQuality;
	switch (rarity) {
		case ItemQuality::JUNK:
		case ItemQuality::COMMON:
			return 200;
		case ItemQuality::RARE:
			return 250;
		case ItemQuality::LEGEND:
			return 300;
		case ItemQuality::UNIQUE:
			return 400;
		case ItemQuality::EPIC:
			return 500;
		case ItemQuality::MYTHIC:
		default:
			return 600;
	}
}

// Java ArmsfusionService.java:118-138
void ArmsfusionService::breakWeapons(model::gameobjects::player::Player& player, int32_t weaponToBreakUniqueId) {
	runtime::Ptr<Item> weaponToBreak = player.getInventory().getItemByObjId(weaponToBreakUniqueId);

	if (weaponToBreak == nullptr) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOUND_ITEM_NO_TARGET_ITEM());
		return;
	}

	if (!weaponToBreak->hasFusionedItem()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOUND_ERROR_NOT_AVAILABLE(weaponToBreak->getL10n()));
		return;
	}

	weaponToBreak->setFusionedItem(nullptr);
	dao::InventoryDAO::store(*weaponToBreak, player);

	item::ItemPacketService::updateItemAfterInfoChange(player, *weaponToBreak);

	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUNDED_ITEM_DECOMPOUND_SUCCESS(weaponToBreak->getL10n()));
}

} // namespace aion::gameserver::services
