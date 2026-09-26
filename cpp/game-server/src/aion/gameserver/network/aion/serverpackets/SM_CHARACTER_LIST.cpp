#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_LIST.h"

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_CHARACTER_LIST::SM_CHARACTER_LIST(int32_t playOk2Value) : AbstractPlayerInfoPacket(opcodeOf<SM_CHARACTER_LIST>), playOk2(playOk2Value) {
}

void SM_CHARACTER_LIST::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_CHARACTER_LIST::writeImpl without a connection");
	runtime::Ptr<model::account::Account> account = con->getAccount();
	writeD(playOk2);
	writeC(account->size()); // character count
	for (runtime::Ptr<model::account::PlayerAccountData> playerData : account->getPlayerAccDataList()) {
		writePlayerInfo(*playerData, con);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
