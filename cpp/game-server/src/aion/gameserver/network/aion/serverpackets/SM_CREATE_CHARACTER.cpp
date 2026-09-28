#include "aion/gameserver/network/aion/serverpackets/SM_CREATE_CHARACTER.h"

#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CREATE_CHARACTER::SM_CREATE_CHARACTER(runtime::Ptr<model::account::PlayerAccountData> accPlData, int32_t responseCodeValue)
	: AbstractPlayerInfoPacket(opcodeOf<SM_CREATE_CHARACTER>), responseCode(responseCodeValue), playerAccData(accPlData) {
}

SM_CREATE_CHARACTER::~SM_CREATE_CHARACTER() = default;

void SM_CREATE_CHARACTER::writeImpl(AionConnection* con) {
	writeD(responseCode);
	if (responseCode != RESPONSE_OK)
		return;
	writePlayerInfo(*runtime::Ptr<model::account::PlayerAccountData>(playerAccData), con);
}

} // namespace aion::gameserver::network::aion::serverpackets
