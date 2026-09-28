#include "aion/gameserver/services/ExchangeService.h"

#include <cstdint>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/trade/Exchange.h"
#include "aion/gameserver/model/trade/ExchangeItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_ADD_KINAH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_CONFIRMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_REQUEST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/services/AdminService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/taskmanager/tasks/TemporaryTradeTimeTask.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::services {

namespace {

/** Java long subtraction (two's complement wrap-around) */
constexpr int64_t javaSub(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b));
}

/**
 * Deviation: D7 (owner decision 2026-09-27; docs/DEVIATIONS.md, docs/deviations/P5-09b.md). The exchange pairs whose trade has started, each
 * by its key exchange: of the pair's two Exchange objects the one of the player with the lower object id, so both partners' confirmations
 * name the same object. confirmExchange adds it before performTrade and removes it when performTrade has returned; Java has no such state.
 * Identity membership (Exchange has no Java equals); the Ref keeps the object, and so its identity, alive while the claim is held.
 */
runtime::ConcurrentKeySet<runtime::Ref<model::trade::Exchange>> tradesStarted{AION_LOCK_CLASS(ExchangeService::tradesStarted#stripe)};

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("EXCHANGE_LOG");

ExchangeService::ExchangeService() = default;

ExchangeService::~ExchangeService() = default;

ExchangeService& ExchangeService::getInstance() {
	static ExchangeService instance; // Java SingletonHolder
	return instance;
}

void ExchangeService::registerExchange(model::gameobjects::player::Player& player1, model::gameobjects::player::Player& player2) {
	if (!validateParticipants(player1, player2))
		return;

	exchanges.put(player1.getObjectId(), model::trade::Exchange::create(player1, player2));
	exchanges.put(player2.getObjectId(), model::trade::Exchange::create(player2, player1));

	utils::PacketSendUtility::sendPacket(player2, network::aion::serverpackets::SM_EXCHANGE_REQUEST(player1.getName()));
	utils::PacketSendUtility::sendPacket(player1, network::aion::serverpackets::SM_EXCHANGE_REQUEST(player2.getName()));
}

bool ExchangeService::validateParticipants(model::gameobjects::player::Player& player1, model::gameobjects::player::Player& player2) {
	return restrictions::PlayerRestrictions::canTrade(runtime::Ptr<model::gameobjects::player::Player>(player1)) &&
		restrictions::PlayerRestrictions::canTrade(runtime::Ptr<model::gameobjects::player::Player>(player2));
}

runtime::Ptr<model::gameobjects::player::Player> ExchangeService::getCurrentParter(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::trade::Exchange> exchange = exchanges.get(player.getObjectId());
	return exchange ? exchange->getTargetPlayer() : nullptr;
}

runtime::Ptr<model::trade::Exchange> ExchangeService::getCurrentExchange(model::gameobjects::player::Player& player) {
	return exchanges.get(player.getObjectId());
}

runtime::Ptr<model::trade::Exchange> ExchangeService::getCurrentParnterExchange(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::gameobjects::player::Player> partner = getCurrentParter(player);
	return partner ? getCurrentExchange(*partner) : nullptr;
}

bool ExchangeService::isPlayerInExchange(model::gameobjects::player::Player& player) {
	return static_cast<bool>(getCurrentExchange(player));
}

void ExchangeService::addKinah(model::gameobjects::player::Player& activePlayer, int64_t itemCount) {
	runtime::Ptr<model::trade::Exchange> currentExchange = getCurrentExchange(activePlayer);
	if (!currentExchange || currentExchange->isLocked())
		return;

	if (itemCount < 1)
		return;

	// count total amount in inventory
	int64_t availableCount = activePlayer.getInventory().getKinah();

	// count amount that was already added to exchange
	availableCount = javaSub(availableCount, currentExchange->getKinahCount());

	int64_t countToAdd = availableCount > itemCount ? itemCount : availableCount;

	if (countToAdd > 0) {
		runtime::Ptr<model::gameobjects::player::Player> partner = getCurrentParter(activePlayer);
		utils::PacketSendUtility::sendPacket(activePlayer, network::aion::serverpackets::SM_EXCHANGE_ADD_KINAH(countToAdd, 0));
		utils::PacketSendUtility::sendPacket(*partner, network::aion::serverpackets::SM_EXCHANGE_ADD_KINAH(countToAdd, 1));
		currentExchange->addKinah(countToAdd);
	}
}

void ExchangeService::addItem(model::gameobjects::player::Player& activePlayer, int32_t itemObjId, int64_t itemCount) {
	runtime::Ptr<model::gameobjects::Item> item = activePlayer.getInventory().getItemByObjId(itemObjId);
	if (!item)
		return;

	runtime::Ptr<model::gameobjects::player::Player> partner = getCurrentParter(activePlayer);
	if (!partner)
		return;
	if (item->getPackCount() <= 0 && !item->isTradeable() &&
		!taskmanager::tasks::TemporaryTradeTimeTask::getInstance().canTrade(*item, partner->getObjectId())) {
		// Java Legion does not override equals: identity
		if (!item->isLegionTradeable() || !activePlayer.getLegion() || activePlayer.getLegion() != partner->getLegion())
			return;
	}

	if (itemCount < 1)
		return;

	if (itemCount > item->getItemCount())
		return;

	runtime::Ptr<model::trade::Exchange> currentExchange = getCurrentExchange(activePlayer);

	if (!currentExchange)
		return;

	if (currentExchange->isLocked())
		return;

	if (currentExchange->isExchangeListFull())
		return;

	if (!AdminService::getInstance().canOperate(activePlayer, partner, *item, "trade"))
		return;

	runtime::Ptr<model::trade::ExchangeItem> exchangeItem = currentExchange->getItems().get(item->getObjectId());

	int64_t actuallAddCount = 0;
	// item was not added previosly
	if (!exchangeItem) {
		runtime::Ref<model::gameobjects::Item> newItem;
		if (itemCount < item->getItemCount()) {
			newItem = services::item::ItemFactory::newItem(item->getItemId(), itemCount);
		} else {
			newItem = runtime::Ref<model::gameobjects::Item>(*item);
		}
		runtime::Ref<model::trade::ExchangeItem> created = model::trade::ExchangeItem::create(itemObjId, itemCount, *newItem);
		exchangeItem = created;
		currentExchange->addItem(itemObjId, *created);
		actuallAddCount = itemCount;
	}
	// item was already added
	else {
		// if player add item count that is more than possible
		// happens with exploits
		if (item->getItemCount() == exchangeItem->getItemCount())
			return;

		int64_t possibleToAdd = javaSub(item->getItemCount(), exchangeItem->getItemCount());
		actuallAddCount = itemCount > possibleToAdd ? possibleToAdd : itemCount;
		exchangeItem->addCount(actuallAddCount);
	}
	static_cast<void>(actuallAddCount); // Java computes it and never reads it

	if (!item->getItemTemplate()->isStackable() || item->getItemCount() == exchangeItem->getItemCount()) {
		utils::PacketSendUtility::sendPacket(activePlayer,
			network::aion::serverpackets::SM_DELETE_ITEM(itemObjId, services::item::ItemPacketService_ItemDeleteType::PUT_TO_EXCHANGE));
	} else {
		runtime::Ref<model::gameobjects::Item> fakeItem = model::gameobjects::Item::create(itemObjId, item->getItemTemplate());
		fakeItem->setItemCount(javaSub(item->getItemCount(), exchangeItem->getItemCount()));
		utils::PacketSendUtility::sendPacket(activePlayer, network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(activePlayer, *fakeItem,
			services::item::ItemPacketService_ItemUpdateType::PUT_TO_EXCHANGE));
	}

	utils::PacketSendUtility::sendPacket(activePlayer, network::aion::serverpackets::SM_EXCHANGE_ADD_ITEM(0, *exchangeItem->getItem(), activePlayer));
	utils::PacketSendUtility::sendPacket(*partner, network::aion::serverpackets::SM_EXCHANGE_ADD_ITEM(1, *exchangeItem->getItem(), *partner));
}

void ExchangeService::lockExchange(model::gameobjects::player::Player& activePlayer) {
	runtime::Ptr<model::trade::Exchange> exchange = getCurrentExchange(activePlayer);
	if (exchange) {
		exchange->lock();
		runtime::Ptr<model::gameobjects::player::Player> currentParter = getCurrentParter(activePlayer);
		utils::PacketSendUtility::sendPacket(*currentParter, network::aion::serverpackets::SM_EXCHANGE_CONFIRMATION(3));
	}
}

void ExchangeService::cancelExchange(model::gameobjects::player::Player& activePlayer) {
	runtime::Ptr<model::gameobjects::player::Player> currentPartner = getCurrentParter(activePlayer);
	returnItems(activePlayer);

	if (currentPartner) {
		returnItems(*currentPartner);
		utils::PacketSendUtility::sendPacket(*currentPartner, network::aion::serverpackets::SM_EXCHANGE_CONFIRMATION(1));
	}

	cleanUpExchanges(true, {runtime::Ptr<model::gameobjects::player::Player>(activePlayer), currentPartner});
}

void ExchangeService::returnItems(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::trade::Exchange> exchange = getCurrentExchange(player);
	if (!exchange) {
		return;
	}
	if (!exchange->getItems().isEmpty()) {
		for (const runtime::Ptr<model::trade::ExchangeItem>& exItem : exchange->getItems().values()) {
			runtime::Ptr<model::gameobjects::Item> realItem = player.getInventory().getItemByObjId(exItem->getItemObjId());
			if (!realItem) {
				log.warn("Player " + player.getName() + " is trying to return fake item on exchange cancel!");
				return;
			}
			if (realItem->getItemCount() == exItem->getItemCount()) {
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_INVENTORY_ADD_ITEM({realItem}, player,
					services::item::ItemPacketService_ItemAddType::PLAYER_EXCHANGE_GET_BACK));
			} else {
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(player, *realItem,
					services::item::ItemPacketService_ItemUpdateType::INC_PLAYER_EXCHANGE_GET_BACK));
			}
		}
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_CUBE_UPDATE::cubeSize(model::items::storage::StorageType::CUBE, player));
	}
}

void ExchangeService::confirmExchange(runtime::Ptr<model::gameobjects::player::Player> activePlayer) {
	if (!activePlayer || !activePlayer->isOnline())
		return;

	runtime::Ptr<model::trade::Exchange> currentExchange = getCurrentExchange(*activePlayer);

	// TODO: Why is exchange null =/
	if (!currentExchange)
		return;
	currentExchange->confirm();

	runtime::Ptr<model::gameobjects::player::Player> currentPartner = getCurrentParter(*activePlayer);
	utils::PacketSendUtility::sendPacket(*currentPartner, network::aion::serverpackets::SM_EXCHANGE_CONFIRMATION(2));

	runtime::Ptr<model::trade::Exchange> partnerExchange = getCurrentExchange(*currentPartner);
	if (partnerExchange->isConfirmed()) {
		// Deviation: D7 (owner decision 2026-09-27; docs/DEVIATIONS.md, docs/deviations/P5-09b.md). Java calls performTrade here unguarded:
		// confirm() above and the partner's isConfirmed() are a check-then-act on plain fields (ExchangeService.java:225-232,
		// Exchange.confirmed), so two CM_EXCHANGE_OK processed at once on the partners' connection threads could both pass the check and both
		// trade - destroying split stacks, paying the kinah twice each way or releasing the object id of a live item (m5c-plan.md D7, §19.4).
		// The two confirmations of one pair are serialized by a compare-and-set on the pair's key exchange (tradesStarted): the one that adds
		// it trades, if the pair it confirmed is still the registered one (a trade that ended meanwhile has removed it); the other returns
		// after its SM_EXCHANGE_CONFIRMATION(2). An unraced confirmation adds the key at once and trades exactly as Java does.
		model::trade::Exchange& pairKey = activePlayer->getObjectId() < currentPartner->getObjectId() ? *currentExchange : *partnerExchange;
		if (!tradesStarted.add(runtime::Ref<model::trade::Exchange>(pairKey)))
			return; // the partner's confirmation is trading this pair
		auto claimed = runtime::finally([&pairKey] { tradesStarted.remove(runtime::Ptr<model::trade::Exchange>(pairKey)); });
		if (getCurrentExchange(*activePlayer) != currentExchange || getCurrentExchange(*currentPartner) != partnerExchange)
			return; // the partner's confirmation traded this pair and released the key before this one took it
		performTrade(*activePlayer, *currentPartner);
	}
}

void ExchangeService::performTrade(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& currentPartner) {
	// Deviation: D7 (see confirmExchange): runs at most once per exchange pair, while its confirmation holds the pair's key in tradesStarted
	runtime::Ptr<model::trade::Exchange> exchange1 = getCurrentExchange(activePlayer);
	runtime::Ptr<model::trade::Exchange> exchange2 = getCurrentExchange(currentPartner);

	if (!validateExchange(activePlayer, currentPartner)) {
		if (!validateInventorySize(currentPartner, *exchange1))
			utils::PacketSendUtility::sendPacket(activePlayer,
				network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_HEAVY_TO_ADD_EXCHANGE_ITEM());
		else
			utils::PacketSendUtility::sendPacket(activePlayer, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_PARTNER_TOO_HEAVY_TO_EXCHANGE());
		cleanUpExchanges(true, {runtime::Ptr<model::gameobjects::player::Player>(activePlayer), runtime::Ptr<model::gameobjects::player::Player>(currentPartner)});
		return;
	}

	if (!removeItemsFromInventory(activePlayer, *exchange1) || !removeItemsFromInventory(currentPartner, *exchange2)) {
		cleanUpExchanges(true, {runtime::Ptr<model::gameobjects::player::Player>(activePlayer), runtime::Ptr<model::gameobjects::player::Player>(currentPartner)});
		utils::audit::AuditLogger::log(activePlayer, "tried to exploit kinah exchange with partner: " + currentPartner.toString());
		return;
	}

	utils::PacketSendUtility::sendPacket(activePlayer, network::aion::serverpackets::SM_EXCHANGE_CONFIRMATION(0));
	utils::PacketSendUtility::sendPacket(currentPartner, network::aion::serverpackets::SM_EXCHANGE_CONFIRMATION(0));

	putItemToInventory(activePlayer, currentPartner, *exchange1, *exchange2);
	putItemToInventory(currentPartner, activePlayer, *exchange2, *exchange1);
	dao::InventoryDAO::store(*exchange1->getActiveplayer());
	dao::InventoryDAO::store(*exchange2->getActiveplayer());

	cleanUpExchanges(false, {runtime::Ptr<model::gameobjects::player::Player>(activePlayer), runtime::Ptr<model::gameobjects::player::Player>(currentPartner)});
}

void ExchangeService::cleanUpExchanges(bool releaseIds, std::initializer_list<runtime::Ptr<model::gameobjects::player::Player>> players) {
	for (const runtime::Ptr<model::gameobjects::player::Player>& player : players) {
		if (!player)
			continue;

		runtime::Ptr<model::trade::Exchange> exchange = exchanges.remove(player->getObjectId());
		if (exchange && releaseIds) {
			for (const runtime::Ptr<model::trade::ExchangeItem>& item : exchange->getItems().values()) {
				if (item->getItemObjId() != item->getItem()->getObjectId() && !player->getInventory().getItemByObjId(item->getItem()->getObjectId()))
					utils::idfactory::IDFactory::getInstance().releaseId(item->getItem()->getObjectId()); // release ID if it was a newly allocated one
			}
		}
	}
}

bool ExchangeService::removeItemsFromInventory(model::gameobjects::player::Player& player, model::trade::Exchange& exchange) {
	model::items::storage::Storage& inventory = player.getInventory();

	for (const runtime::Ptr<model::trade::ExchangeItem>& exchangeItem : exchange.getItems().values()) {
		runtime::Ptr<model::gameobjects::Item> item = exchangeItem->getItem();
		runtime::Ptr<model::gameobjects::Item> itemInInventory = inventory.getItemByObjId(exchangeItem->getItemObjId());
		if (!itemInInventory) {
			utils::audit::AuditLogger::log(player, "tried to trade not existing item");
			return false;
		}

		int64_t itemCount = exchangeItem->getItemCount();

		if (itemCount < itemInInventory->getItemCount()) {
			inventory.decreaseItemCount(*itemInInventory, itemCount);
		} else {
			// remove from source inventory only
			inventory.remove(*itemInInventory);
			exchangeItem->setItem(itemInInventory);
			// release when only part stack was added in the beginning -> full stack in the end
			if (item->getObjectId() != exchangeItem->getItemObjId()) {
				utils::idfactory::IDFactory::getInstance().releaseId(item->getObjectId());
			}
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DELETE_ITEM(itemInInventory->getObjectId()));
		}
	}
	return player.getInventory().tryDecreaseKinah(exchange.getKinahCount());
}

bool ExchangeService::validateExchange(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& currentPartner) {
	runtime::Ptr<model::trade::Exchange> exchange1 = getCurrentExchange(activePlayer);
	runtime::Ptr<model::trade::Exchange> exchange2 = getCurrentExchange(currentPartner);
	bool activePlayerCheck = validateInventorySize(activePlayer, *exchange2);
	bool currentPartnerCheck = validateInventorySize(currentPartner, *exchange1);
	if (!activePlayerCheck) {
		utils::PacketSendUtility::sendPacket(activePlayer,
			network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_HEAVY_TO_ADD_EXCHANGE_ITEM());
		utils::PacketSendUtility::sendPacket(currentPartner, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_PARTNER_TOO_HEAVY_TO_EXCHANGE());
	} else if (!currentPartnerCheck) {
		utils::PacketSendUtility::sendPacket(currentPartner,
			network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_HEAVY_TO_ADD_EXCHANGE_ITEM());
		utils::PacketSendUtility::sendPacket(activePlayer, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_PARTNER_TOO_HEAVY_TO_EXCHANGE());
	}
	return activePlayerCheck && currentPartnerCheck;
}

bool ExchangeService::validateInventorySize(model::gameobjects::player::Player& activePlayer, model::trade::Exchange& exchange) {
	int32_t numberOfFreeSlots = activePlayer.getInventory().getFreeSlots();
	return numberOfFreeSlots >= exchange.getItems().size();
}

void ExchangeService::putItemToInventory(model::gameobjects::player::Player& giver, model::gameobjects::player::Player& partner, model::trade::Exchange& exchange1, model::trade::Exchange& exchange2) {
	static_cast<void>(exchange2); // Java's parameter is never read
	for (const runtime::Ptr<model::trade::ExchangeItem>& exchangeItem : exchange1.getItems().values()) {
		runtime::Ptr<model::gameobjects::Item> itemToPut = exchangeItem->getItem();
		itemToPut->setEquipmentSlot(0);
		if (itemToPut->getPackCount() > 0) // unpack
			itemToPut->setPackCount(itemToPut->getPackCount() * -1);
		partner.getInventory().add(*itemToPut, services::item::ItemPacketService_ItemAddType::PLAYER_EXCHANGE_GET);
		if (configs::main::LoggingConfig::LOG_PLAYER_EXCHANGE.load())
			log.info("Player " + giver.getName() + " exchanged item " + std::to_string(itemToPut->getItemId()) + " [" + itemToPut->getItemName()
				+ "] (count: " + std::to_string(itemToPut->getItemCount()) + ") with player " + partner.getName());
	}
	int64_t kinahToExchange = exchange1.getKinahCount();
	if (kinahToExchange > 0) {
		partner.getInventory().increaseKinah(kinahToExchange);
		if (configs::main::LoggingConfig::LOG_PLAYER_EXCHANGE.load())
			log.info("Player " + giver.getName() + " exchanged " + std::to_string(kinahToExchange) + " Kinah with player " + partner.getName());
	}
}

} // namespace aion::gameserver::services
