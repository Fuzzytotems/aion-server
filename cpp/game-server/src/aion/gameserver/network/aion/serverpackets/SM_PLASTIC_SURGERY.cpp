#include "aion/gameserver/network/aion/serverpackets/SM_PLASTIC_SURGERY.h"

#include <array>
#include <span>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/**
 * Java: CM_CHARACTER_EDIT.checkOrRemoveTicket(player, isGenderSwitch, false). CM_CHARACTER_EDIT.h is a file of P5-00/P5-15 that does not exist
 * yet; the check without removal only reads the inventory, so it stands in here with the Java ticket ids. TODO(P5-15): call the client packet's
 * static once it is ported.
 */
bool checkTicket(model::gameobjects::player::Player& player, bool isGenderSwitch) {
	static constexpr std::array<int32_t, 5> GENDER_SWITCH_TICKETS{169660000, 169660001, 169660002, 169660003, 169660004};
	static constexpr std::array<int32_t, 9> PLASTIC_SURGERY_TICKETS{169650000, 169650001, 169650002, 169650003, 169650004, 169650005, 169650006,
		169650007, 169650008};
	const std::span<const int32_t> ticketIds =
		isGenderSwitch ? std::span<const int32_t>(GENDER_SWITCH_TICKETS) : std::span<const int32_t>(PLASTIC_SURGERY_TICKETS);
	for (int32_t ticketId : ticketIds) {
		if (player.getInventory().getItemCountByItemId(ticketId) > 0)
			return true;
	}
	return false;
}

} // namespace

SM_PLASTIC_SURGERY::SM_PLASTIC_SURGERY(model::gameobjects::player::Player& player, bool isGenderSwitchValue)
	: AionServerPacket(opcodeOf<SM_PLASTIC_SURGERY>), isGenderSwitch(isGenderSwitchValue) {
	playerObjId = player.getObjectId();
	hasTicket = checkTicket(player, isGenderSwitchValue);
}

void SM_PLASTIC_SURGERY::writeImpl(AionConnection* con) {
	writeD(playerObjId);
	writeC(hasTicket ? 1 : 2);
	writeC(isGenderSwitch ? 1 : 0);
}

} // namespace aion::gameserver::network::aion::serverpackets
