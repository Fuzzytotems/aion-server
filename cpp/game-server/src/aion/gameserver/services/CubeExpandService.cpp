#include "aion/gameserver/services/CubeExpandService.h"

#include <algorithm>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/CubeExpandData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/templates/StorageExpansionTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.CubeExpandService");

namespace {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::gameobjects::player::RequestResponseHandler;
using network::aion::serverpackets::SM_CUBE_UPDATE;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/**
 * Java: the anonymous RequestResponseHandler<Npc> of expandCube (CubeExpandService.java:50-58, fieldmap key CubeExpandService$1), stored in the
 * player's ResponseRequester until he answers STR_WAREHOUSE_EXPAND_WARNING. It captures the boxed price; the requester (the cube expander) is
 * the base's.
 */
class CubeExpandService_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const std::optional<int32_t> price; // captured local Integer price (line 54) [captured variable: final nullable scalar]

	static runtime::Ref<CubeExpandService_RequestResponseHandler> create(Npc& npc, std::optional<int32_t> priceValue) {
		return runtime::makeRef<CubeExpandService_RequestResponseHandler>(npc, priceValue);
	}

	// Java CubeExpandService.java:52-57
	void acceptRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		// Java unboxes the Integer; expandCube never creates the handler for a null price, which would be its NullPointerException
		if (!price)
			throw runtime::NullPointerException("price");
		if (responder.getInventory().tryDecreaseKinah(*price, item::ItemPacketService_ItemUpdateType::DEC_KINAH_CUBE))
			CubeExpandService::npcExpand(responder);
		else
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY()); // warehouse and cube use the same msg..
	}

protected:
	CubeExpandService_RequestResponseHandler(Npc& npc, std::optional<int32_t> priceValue)
		: RequestResponseHandler(runtime::Ptr<Creature>(npc)), price(priceValue) {}
	~CubeExpandService_RequestResponseHandler() override = default;
};

} // namespace

// Java CubeExpandService.java:29-64
void CubeExpandService::expandCube(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	const model::templates::StorageExpansionTemplate* expansionTemplate =
		dataholders::DataManager::CUBEEXPANDER_DATA->getCubeExpansionTemplate(npc.getNpcId());
	if (expansionTemplate == nullptr) {
		log.warn("Cube expansion template could not be found for " + npc.toString());
		return;
	}

	if (!canExpand(player))
		return;
	int32_t newNpcExpansions = player.getNpcExpands() + 1;
	int32_t minExpansionLevel = expansionTemplate->getMinExpansionLevel();
	if (newNpcExpansions < minExpansionLevel) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_DUE_TO_MINIMUM_EXTEND_LEVEL_BY_THIS_NPC(
			npc.getObjectTemplate()->getL10n(), minExpansionLevel - 1));
		return;
	}
	std::optional<int32_t> price = expansionTemplate->getPrice(newNpcExpansions);
	int32_t maxExpansionLevel = std::min(expansionTemplate->getMaxExpansionLevel(), configs::main::CustomConfig::NPC_CUBE_EXPANDS_SIZE_LIMIT.load());
	if (!price || newNpcExpansions > maxExpansionLevel) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC(
			npc.getObjectTemplate()->getL10n(), maxExpansionLevel));
		return;
	}
	runtime::Ref<CubeExpandService_RequestResponseHandler> responseHandler = CubeExpandService_RequestResponseHandler::create(npc, price);
	bool result = player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, responseHandler);
	if (result) {
		// Java String.valueOf(price) of the Integer
		PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, 0, 0, std::to_string(*price)));
	}
}

// Java CubeExpandService.java:73-92
void CubeExpandService::expand(model::gameobjects::player::Player& player, int32_t type) {
	if (!canExpand(player))
		return;
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_SIZE_EXTENDED(9));
	switch (type) {
		case 1: // npc
			player.getCommonData()->setNpcExpands(player.getNpcExpands() + 1);
			break;
		case 2: // item
			player.getCommonData()->setItemExpands(player.getItemExpands() + 1);
			break;
		case 3: // quest
			player.getCommonData()->setQuestExpands(player.getQuestExpands() + 1);
			break;
		default:
			break;
	}
	player.setCubeLimit();
	PacketSendUtility::sendPacket(player, SM_CUBE_UPDATE::cubeSize(model::items::storage::StorageType::CUBE, player));
}

void CubeExpandService::questExpand(model::gameobjects::player::Player& player) {
	expand(player, 3);
}

void CubeExpandService::itemExpand(model::gameobjects::player::Player& player) {
	expand(player, 2);
}

void CubeExpandService::npcExpand(model::gameobjects::player::Player& player) {
	expand(player, 1);
}

// Java CubeExpandService.java:106-114
bool CubeExpandService::canExpandByTicket(model::gameobjects::player::Player& player, int32_t ticketLevel) {
	if (!canExpand(player))
		return false;
	if (player.getItemExpands() >= ticketLevel) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE());
		return false;
	}
	return true;
}

// Java CubeExpandService.java:116-125
bool CubeExpandService::canExpand(model::gameobjects::player::Player& player) {
	// Java int addition: the sum wraps like Java's (a negative sum is refused below)
	int32_t newExpansions = static_cast<int32_t>(static_cast<uint32_t>(player.getNpcExpands()) + static_cast<uint32_t>(player.getQuestExpands()) +
		static_cast<uint32_t>(player.getItemExpands()) + 1u);
	if (newExpansions < 0)
		return false;
	if (newExpansions > configs::main::CustomConfig::CUBE_EXPANSION_LIMIT.load()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE());
		return false;
	}
	return true;
}

} // namespace aion::gameserver::services
