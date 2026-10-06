#include "aion/gameserver/services/item/ItemPurificationService.h"

#include <algorithm>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemPurificationData.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/TuningAction.h"
#include "aion/gameserver/model/templates/item/bonuses/StatBonusType.h"
#include "aion/gameserver/model/templates/item/purification/PurificationResult.h"
#include "aion/gameserver/model/templates/item/purification/RequiredMaterial.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services::item {

namespace {

using model::gameobjects::Item;
using model::templates::item::purification::PurificationResult;
using model::templates::item::purification::RequiredMaterial;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using utils::PacketSendUtility;

const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.item.ItemPurificationService");

/** Java: DataManager.ITEM_PURIFICATION_DATA.getResultItemMap(baseItemId).get(resultItemId) - a null map is a NullPointerException */
const PurificationResult* resultOf(int32_t baseItemId, int32_t resultItemId) {
	const dataholders::ItemPurificationData::ResultItemMap* resultItemMap = dataholders::DataManager::ITEM_PURIFICATION_DATA->getResultItemMap(baseItemId);
	if (resultItemMap == nullptr)
		throw runtime::NullPointerException("ItemPurificationData.getResultItemMap(" + std::to_string(baseItemId) + ")");
	auto it = resultItemMap->find(resultItemId);
	return it == resultItemMap->end() ? nullptr : it->second;
}

} // namespace

// Java ItemPurificationService.java:29-71
bool ItemPurificationService::isPurificationAllowed(model::gameobjects::player::Player& player, model::gameobjects::Item& baseItem, int32_t resultItemId) {
	if (dataholders::DataManager::ITEM_PURIFICATION_DATA->getItemPurificationTemplate(baseItem.getItemId()) == nullptr) {
		log.warn("Item purification template is not available for [resultItemId=" + std::to_string(resultItemId) + "]");
		return false;
	}

	const PurificationResult* purificationResult = resultOf(baseItem.getItemId(), resultItemId);
	if (purificationResult == nullptr) {
		utils::audit::AuditLogger::log(player, "tried to purify an item to an invalid result [baseItemId=" + std::to_string(baseItem.getItemId())
			+ ", resultItemId=" + std::to_string(resultItemId) + "]");
		return false;
	}

	if (!baseItem.isIdentified()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REGISTER_ITEM_MSG_UPGRADE_CANNOT_NO_IDENTIFY());
		return false;
	}

	if (baseItem.getEnchantLevel() < purificationResult->getMinEnchantCount()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REGISTER_ITEM_MSG_UPGRADE_CANNOT(baseItem.getL10n()));
		return false;
	}

	if (player.getAbyssRank()->getAp() < purificationResult->getNecessaryAbyssPoints()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REGISTER_ITEM_MSG_UPGRADE_CANNOT_NEED_AP());
		return false;
	}

	if (player.getInventory().getKinah() < purificationResult->getNecessaryKinah()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REGISTER_ITEM_MSG_UPGRADE_CANNOT_NEED_QINA());
		return false;
	}

	for (const RequiredMaterial& reqMat : purificationResult->getRequiredMaterials())
		if (player.getInventory().getItemCountByItemId(reqMat.getItemId()) < reqMat.getItemCount())
			return false;

	const model::templates::item::ItemTemplate* resultTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(resultItemId);
	if (resultTemplate == nullptr) // Java: getItemTemplate(resultItemId).getL10n() on null
		throw runtime::NullPointerException("ItemData.getItemTemplate(" + std::to_string(resultItemId) + ")");
	std::string resultItemL10n = resultTemplate->getL10n();
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_UPGRADE_MSG_UPGRADE_SUCCESS(baseItem.getL10n(), resultItemL10n));
	return true;
}

// Java ItemPurificationService.java:73-95
bool ItemPurificationService::decreaseMaterials(model::gameobjects::player::Player& player, model::gameobjects::Item& baseItem, int32_t resultItemId) {
	const PurificationResult* purificationResult = resultOf(baseItem.getItemId(), resultItemId);
	if (purificationResult == nullptr) // Java: purificationResult.getRequiredMaterials() on null
		throw runtime::NullPointerException("PurificationResult of " + std::to_string(resultItemId));

	for (const RequiredMaterial& reqMaterial : purificationResult->getRequiredMaterials()) {
		if (!player.getInventory().decreaseByItemId(reqMaterial.getItemId(), reqMaterial.getItemCount())) {
			utils::audit::AuditLogger::log(player, "tried to use item purification with insufficient materials [baseItemId="
				+ std::to_string(baseItem.getItemId()) + ", resultItemId=" + std::to_string(resultItemId) + ", reqMaterialId="
				+ std::to_string(reqMaterial.getItemId()) + ", reqMaterialCount=" + std::to_string(reqMaterial.getItemCount()) + "]");
			return false;
		}
	}

	if (purificationResult->getNecessaryAbyssPoints() > 0)
		abyss::AbyssPointsService::addAp(player, -purificationResult->getNecessaryAbyssPoints());

	// java-bug kept (docs/deviations/P5-07.md, proposed correction): decreaseKinah of a negative amount does nothing (Storage.decreaseKinah
	// decreases only amount > 0), so the purification never takes its kinah
	if (purificationResult->getNecessaryKinah() > 0)
		player.getInventory().decreaseKinah(-purificationResult->getNecessaryKinah());

	player.getInventory().decreaseByObjectId(baseItem.getObjectId(), 1);

	return true;
}

// Java ItemPurificationService.java:97-140
void ItemPurificationService::upgradeItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int32_t targetItemId) {
	runtime::Ref<Item> newItemRef = ItemFactory::newItem(targetItemId, 1);
	Item& newItem = *newItemRef;
	newItem.setOptionalSockets(sourceItem.getOptionalSockets());
	newItem.setItemCreator(sourceItem.getItemCreator());
	newItem.setTuneCount(std::max(0, std::min(sourceItem.getTuneCount(), newItem.getItemTemplate()->getMaxTuneCount())));
	newItem.setEnchantLevel(static_cast<int32_t>(static_cast<uint32_t>(sourceItem.getEnchantLevel()) - 5U));
	newItem.setEnchantBonus(sourceItem.getEnchantBonus());
	newItem.setAmplified(sourceItem.isAmplified() && newItem.getEnchantLevel() >= newItem.getMaxEnchantLevel());
	if (newItem.isAmplified() && newItem.getEnchantLevel() >= 20) {
		newItem.setBuffSkill(sourceItem.getBuffSkill());
	}
	if (sourceItem.hasFusionedItem()) {
		newItem.setFusionedItem(sourceItem.getFusionedItemTemplate(), sourceItem.getFusionedItemBonusStatsId(), sourceItem.getFusionedItemOptionalSockets());
	}
	if (sourceItem.hasManaStones()) {
		for (const Ptr<model::items::ManaStone>& manaStone : sourceItem.getItemStones()->snapshot())
			ItemSocketService::addManaStone(Ptr<Item>(newItem), manaStone->getItemId(), false);
	}
	if (sourceItem.hasFusionStones()) {
		for (const Ptr<model::items::ManaStone>& manaStone : sourceItem.getFusionStones()->snapshot())
			ItemSocketService::addManaStone(Ptr<Item>(newItem), manaStone->getItemId(), true);
	}
	if (sourceItem.getGodStone() != nullptr)
		newItem.addGodStone(sourceItem.getGodStone()->getItemId(), sourceItem.getGodStone()->getActivatedCount());
	if (sourceItem.getTempering() > 0)
		newItem.setTempering(sourceItem.getTempering());
	if (sourceItem.isSoulBound())
		newItem.setSoulBound(true);
	if (sourceItem.getBonusStatsId() > 0) {
		int32_t statBonusId = sourceItem.getBonusStatsId();
		if (!dataholders::DataManager::ITEM_RANDOM_BONUSES->areBonusSetsEqual(model::templates::item::bonuses::StatBonusType::INVENTORY,
				sourceItem.getItemTemplate()->getStatBonusSetId(), newItem.getItemTemplate()->getStatBonusSetId())) {
			statBonusId = model::templates::item::actions::TuningAction::getRandomStatBonusIdFor(newItem);
		}
		newItem.setBonusStats(statBonusId, true);
	}
	newItem.setItemColor(sourceItem.getItemColor());
	player.getInventory().add(newItem);
}

} // namespace aion::gameserver::services::item
