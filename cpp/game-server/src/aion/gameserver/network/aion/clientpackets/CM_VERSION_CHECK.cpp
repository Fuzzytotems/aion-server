#include "aion/gameserver/network/aion/clientpackets/CM_VERSION_CHECK.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_VERSION_CHECK.h"
#include "aion/gameserver/services/event/EventService.h"

namespace aion::gameserver::network::aion::clientpackets {

CM_VERSION_CHECK::CM_VERSION_CHECK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_VERSION_CHECK::readImpl() {
	aionClientVersion = readUH();
	npcScriptInterfaceVersion = readUH();
	windowsEncoding = readD();
	windowsVersion = readD();
	windowsSubVersion = readD();
	liteInfo = readC(); // info if client is fully downloaded? seen values: 1, 2 (client checks for "2-ESSENTIAL" in data\lite\LiteGroupOrder.xml)
}

void CM_VERSION_CHECK::runImpl() {
	sendPacket(serverpackets::SM_VERSION_CHECK(aionClientVersion, services::event::EventService::getInstance().getEventTheme()));
}

AION_CLIENT_PACKET(CM_VERSION_CHECK);

} // namespace aion::gameserver::network::aion::clientpackets
