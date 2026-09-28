#include "aion/gameserver/network/aion/clientpackets/CM_TIME_CHECK.h"

#include <memory>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AFTER_TIME_CHECK_4_7_5.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TIME_CHECK.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_TIME_CHECK::CM_TIME_CHECK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_TIME_CHECK::readImpl() {
	nanoTime = readD();
}

void CM_TIME_CHECK::runImpl() {
	// int timeNow = (int) (System.nanoTime() / 1000000);
	// int diff = timeNow - nanoTime;
	// System.out.println("CM_TIME_CHECK: " + nanoTime + " =?= " + timeNow + " dif: " + diff);
	const std::shared_ptr<AionConnection>& con = getConnection();
	con->sendPacket(serverpackets::SM_AFTER_TIME_CHECK_4_7_5()); // don't know what is this doing it is send after this on retail
	con->sendPacket(serverpackets::SM_TIME_CHECK(nanoTime));
}

AION_CLIENT_PACKET(CM_TIME_CHECK);

} // namespace aion::gameserver::network::aion::clientpackets
