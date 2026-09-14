#include "aion/gameserver/network/aion/serverpackets/SM_GM_SEARCH.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_SEARCH::SM_GM_SEARCH(model::gameobjects::player::Player& playerValue) : AionServerPacket(opcodeOf<SM_GM_SEARCH>), player(playerValue) {
}

SM_GM_SEARCH::~SM_GM_SEARCH() = default;

void SM_GM_SEARCH::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
