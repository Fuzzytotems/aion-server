#include "aion/gameserver/network/aion/clientpackets/CM_BONUS_TITLE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_BONUS_TITLE::CM_BONUS_TITLE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BONUS_TITLE.java:21-23
void CM_BONUS_TITLE::readImpl() {
	bonusTitleId = readUH();
}

// Java CM_BONUS_TITLE.java:26-33
void CM_BONUS_TITLE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (bonusTitleId != 0xFFFF) // 0xFFFF: no bonus title
		if (!player->getTitleList().contains(bonusTitleId))
			return;
	player->getTitleList().setBonusTitle(bonusTitleId);
}

AION_CLIENT_PACKET(CM_BONUS_TITLE);

} // namespace aion::gameserver::network::aion::clientpackets
