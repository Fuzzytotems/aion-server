#include "aion/gameserver/network/aion/clientpackets/CM_RESTORE_CHARACTER.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RESTORE_CHARACTER.h"
#include "aion/gameserver/services/player/PlayerService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_RESTORE_CHARACTER::CM_RESTORE_CHARACTER(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_RESTORE_CHARACTER::readImpl() {
	playOk2 = readD();
	chaOid = readD();
}

void CM_RESTORE_CHARACTER::runImpl() {
	runtime::Ptr<model::account::Account> account = getConnection()->getAccount();
	runtime::Ptr<model::account::PlayerAccountData> pad = account->getPlayerAccountData(chaOid);
	bool success = pad && services::player::PlayerService::cancelPlayerDeletion(*pad);
	sendPacket(serverpackets::SM_RESTORE_CHARACTER(chaOid, success));
}

AION_CLIENT_PACKET(CM_RESTORE_CHARACTER);

} // namespace aion::gameserver::network::aion::clientpackets
