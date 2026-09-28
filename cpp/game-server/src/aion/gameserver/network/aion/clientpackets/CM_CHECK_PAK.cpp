#include "aion/gameserver/network/aion/clientpackets/CM_CHECK_PAK.h"

#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHECK_PAK::CM_CHECK_PAK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_CHECK_PAK::readImpl() {
	unk = readC(); // 2
	pakStatus = readS();
}

void CM_CHECK_PAK::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	// Java String.endsWith/contains with ASCII literals: byte-wise on the UTF-8 std::string gives the same answer, because an ASCII needle can
	// never match inside a multi-byte UTF-8 sequence.
	if (!pakStatus.empty() && !pakStatus.ends_with("[1:OK]") && pakStatus.find("File not found") == std::string::npos)
		utils::audit::AuditLogger::log(*player, "using modified data pak: " + pakStatus);
}

AION_CLIENT_PACKET(CM_CHECK_PAK);

} // namespace aion::gameserver::network::aion::clientpackets
