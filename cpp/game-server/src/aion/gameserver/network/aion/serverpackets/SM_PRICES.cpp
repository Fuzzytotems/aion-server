#include "aion/gameserver/network/aion/serverpackets/SM_PRICES.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PRICES::SM_PRICES()
	: AionServerPacket(opcodeOf<SM_PRICES>) {
}

void SM_PRICES::writeImpl(AionConnection* con) {
	AionConnection& connection = detail::requireConnection(con, "SM_PRICES");
	writeC(detail::getGlobalPrices(connection.getActivePlayer()->getRace())); // Display Buying Price %
	writeC(detail::getGlobalPricesModifier()); // Buying Modified Price %
	writeC(detail::getTaxes(connection.getActivePlayer()->getRace())); // Tax = -100 + C %
}

} // namespace aion::gameserver::network::aion::serverpackets
