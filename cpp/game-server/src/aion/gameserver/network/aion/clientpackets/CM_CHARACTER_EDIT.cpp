#include "aion/gameserver/network/aion/clientpackets/CM_CHARACTER_EDIT.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dao/PlayerAppearanceDAO.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/player/PlayerEnterWorldService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHARACTER_EDIT::CM_CHARACTER_EDIT(int32_t opcode, const StateSet& validStates) : AbstractCharacterEditPacket(opcode, validStates) {
}

// Java CM_CHARACTER_EDIT.java:31-36
void CM_CHARACTER_EDIT::readImpl() {
	objectId = readD();
	readBasicInfo(false);
	readAppearance();
}

// Java CM_CHARACTER_EDIT.java:38-60
void CM_CHARACTER_EDIT::runImpl() {
	AionConnection* client = getConnection().get();
	runtime::Ptr<model::account::PlayerAccountData> playerAccData = client->getAccount()->getPlayerAccountData(objectId);
	if (playerAccData == nullptr || !playerAccData->getPlayerCommonData()->isInEditMode())
		return;

	services::player::PlayerEnterWorldService::enterWorld(client, objectId);
	runtime::Ptr<model::gameobjects::player::Player> player = client->getActivePlayer();
	bool isGenderSwitch = player->getGender() != gender;
	if (checkOrRemoveTicket(*player, isGenderSwitch, true)) {
		bool spawnedBeforeAttributesChanged = player->isSpawned(); // just in case CM_LEVEL_READY was sent early
		if (isGenderSwitch)
			player->getCommonData()->setGender(gender); // no need to save gender here, will be saved periodically and on logout
		player->setPlayerAppearance(*playerAppearance);
		dao::PlayerAppearanceDAO::store(*player); // save new appearance
		if (spawnedBeforeAttributesChanged)
			player->getController().onChangedPlayerAttributes();
	} else { // can only happen if you illegally enter the character edit screen
		utils::audit::AuditLogger::log(*player, "tried to apply their plastic surgery without a ticket.");
	}
}

// Java CM_CHARACTER_EDIT.java:62-72
bool CM_CHARACTER_EDIT::checkOrRemoveTicket(model::gameobjects::player::Player& player, bool isGenderSwitch, bool removeTicket) {
	static constexpr std::array<int32_t, 5> GENDER_SWITCH_TICKETS{169660000, 169660001, 169660002, 169660003, 169660004};
	static constexpr std::array<int32_t, 9> PLASTIC_SURGERY_TICKETS{169650000, 169650001, 169650002, 169650003, 169650004, 169650005, 169650006,
		169650007, 169650008};
	const std::span<const int32_t> ticketIds =
		isGenderSwitch ? std::span<const int32_t>(GENDER_SWITCH_TICKETS) : std::span<const int32_t>(PLASTIC_SURGERY_TICKETS);
	for (int32_t ticketId : ticketIds) {
		if (removeTicket && player.getInventory().decreaseByItemId(ticketId, 1) || !removeTicket && player.getInventory().getItemCountByItemId(ticketId) > 0) {
			return true;
		}
	}
	return false;
}

AION_CLIENT_PACKET(CM_CHARACTER_EDIT);

} // namespace aion::gameserver::network::aion::clientpackets
