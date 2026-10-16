#include "aion/gameserver/network/aion/clientpackets/CM_BUY_ITEM.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/pet/PetFunction.h"
#include "aion/gameserver/model/templates/pet/PetFunctionType.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/tradelist/TradeNpcType.h"
#include "aion/gameserver/model/trade/RepurchaseList.h"
#include "aion/gameserver/model/trade/TradeList.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/services/PrivateStoreService.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/services/TradeService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Npc;
using model::gameobjects::Pet;
using model::gameobjects::player::Player;
using model::templates::tradelist::TradeListTemplate;
using model::templates::tradelist::TradeNpcType;
using utils::audit::AuditLogger;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_BUY_ITEM");

CM_BUY_ITEM::CM_BUY_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BUY_ITEM.java:47-90
void CM_BUY_ITEM::readImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	sellerObjId = readD();
	tradeActionId = readH();
	amount = readUH(); // total no of items

	if (amount < 0 || amount > 36) {
		isAudit = true;
		AuditLogger::log(player, "might be abusing CM_BUY_ITEM amount: " + std::to_string(amount));
		return;
	}
	if (tradeActionId == 2) {
		repurchaseList = model::trade::RepurchaseList::create(sellerObjId);
	} else {
		tradeList = model::trade::TradeList::create(sellerObjId);
	}

	for (int32_t i = 0; i < amount; i++) {
		itemId = readD();
		count = readQ();

		// prevent exploit packets
		if (count < 0 || (itemId <= 0 && tradeActionId != 0) || count > 20000) {
			isAudit = true;
			AuditLogger::log(player, "might be abusing CM_BUY_ITEM item: " + std::to_string(itemId) + " count: " + std::to_string(count));
			break;
		}

		switch (tradeActionId) {
			case 0: // private store (in this case its not itemId/objId, but item index in sellers list...)
			case 1: // sell to shop
			case 13: // buy from shop
			case 14: // buy from abyss shop
			case 15: // buy from reward shop
			case 16: // buy from general shop
			case 17: // sell to pet
				tradeList->addItem(itemId, count);
				break;
			case 2: // repurchase
				repurchaseList->addRepurchaseItem(*player, itemId, count);
				break;
		}
	}
}

// Java CM_BUY_ITEM.java:93-143
void CM_BUY_ITEM::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();

	if (isAudit || !player)
		return;

	const runtime::Ptr<model::gameobjects::VisibleObject> target = player->getKnownList().getObject(sellerObjId);

	if (!target)
		return;

	const runtime::Ptr<Player> targetPlayer = runtime::as<Player>(target);
	if (targetPlayer && tradeActionId == 0) {
		services::PrivateStoreService::sellStoreItem(*targetPlayer, *player, *tradeList);
	} else if (const runtime::Ptr<Npc> npc = runtime::as<Npc>(target)) {
		if (!services::DialogService::isInteractionAllowed(*player, *npc)) {
			AuditLogger::log(*player, "might be abusing CM_BUY_ITEM: no right trading with " + npc->toString());
			return;
		}
		const TradeListTemplate* tradeTemplate = nullptr;
		switch (tradeActionId) {
			case 1: // sell to shop
				if (npc->canBuy() || npc->canPurchase()) {
					tradeTemplate = dataholders::DataManager::TRADE_LIST_DATA->getPurchaseTemplate(npc->getNpcId());
					if (tradeTemplate != nullptr && tradeTemplate->getTradeNpcType() == TradeNpcType::ABYSS)
						services::TradeService::performSellForAPToShop(*player, *tradeList, tradeTemplate);
					else
						services::TradeService::performSellToShop(*player, *tradeList, tradeTemplate);
				}
				break;
			case 2: // repurchase
				if (npc->canBuy())
					services::RepurchaseService::getInstance().repurchaseFromShop(*player, *repurchaseList);
				break;
			case 13: // buy from shop
			case 14: // buy from abyss shop
			case 15: // reward shop
			case 16: // abyss_kinah shop
				if (npc->canSell())
					services::TradeService::performBuyFromShop(*npc, *player, *tradeList);
				break;
			default:
				log.warn("Unknown shop action: " + std::to_string(tradeActionId));
				break;
		}
	} else if (const runtime::Ptr<Pet> pet = runtime::as<Pet>(target)) {
		const model::templates::pet::PetFunction* pf = pet->getObjectTemplate()->getPetFunction(model::templates::pet::PetFunctionType::MERCHANT);
		if (pf != nullptr && tradeActionId == 17) {
			services::TradeService::performSellToShop(*player, *tradeList, nullptr, pf->getRatePrice());
		}
	}
}

AION_CLIENT_PACKET(CM_BUY_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
