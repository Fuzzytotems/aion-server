#include "aion/gameserver/network/aion/serverpackets/SM_GM_SEARCH.h"

#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_SEARCH::SM_GM_SEARCH(model::gameobjects::player::Player& playerValue) : AionServerPacket(opcodeOf<SM_GM_SEARCH>), player(playerValue) {
}

SM_GM_SEARCH::~SM_GM_SEARCH() = default;

void SM_GM_SEARCH::writeImpl(AionConnection* con) {
	using model::templates::detail::floatToInt;
	writeS("search " + player->getName() + " " + std::to_string(player->getWorldId()) + " " + std::to_string(floatToInt(player->getX())) + " " +
		std::to_string(floatToInt(player->getY())) + " " + std::to_string(floatToInt(player->getZ())));
}

} // namespace aion::gameserver::network::aion::serverpackets
