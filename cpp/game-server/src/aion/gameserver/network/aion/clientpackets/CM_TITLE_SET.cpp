#include "aion/gameserver/network/aion/clientpackets/CM_TITLE_SET.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_TITLE_SET::CM_TITLE_SET(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_TITLE_SET.java:21-23
void CM_TITLE_SET::readImpl() {
	titleId = readUH();
}

// Java CM_TITLE_SET.java:26-33
void CM_TITLE_SET::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (titleId != 0xFFFF) // 0xFFFF: no title
		if (!player->getTitleList().contains(titleId))
			return;
	player->getTitleList().setDisplayTitle(titleId);
}

AION_CLIENT_PACKET(CM_TITLE_SET);

} // namespace aion::gameserver::network::aion::clientpackets
