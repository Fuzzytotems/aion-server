#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/trade/Exchange.h"
#include "aion/gameserver/model/trade/ExchangeItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_CONFIRMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("EXCHANGE_LOG");

ExchangeService::ExchangeService() = default;

ExchangeService::~ExchangeService() = default;

ExchangeService& ExchangeService::getInstance() {
	static ExchangeService instance; // Java SingletonHolder
	return instance;
}

void ExchangeService::registerExchange(model::gameobjects::player::Player& player1, model::gameobjects::player::Player& player2) {
	AION_UNPORTED();
}

bool ExchangeService::validateParticipants(model::gameobjects::player::Player& player1, model::gameobjects::player::Player& player2) {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

void ExchangeService::addItem(model::gameobjects::player::Player& activePlayer, int32_t itemObjId, int64_t itemCount) {
	AION_UNPORTED();
}

void ExchangeService::lockExchange(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

void ExchangeService::performTrade(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& currentPartner) {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

bool ExchangeService::validateExchange(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& currentPartner) {
	AION_UNPORTED();
}

bool ExchangeService::validateInventorySize(model::gameobjects::player::Player& activePlayer, model::trade::Exchange& exchange) {
	AION_UNPORTED();
}

void ExchangeService::putItemToInventory(model::gameobjects::player::Player& giver, model::gameobjects::player::Player& partner, model::trade::Exchange& exchange1, model::trade::Exchange& exchange2) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
