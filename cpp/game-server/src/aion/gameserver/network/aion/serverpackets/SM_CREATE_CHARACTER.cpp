#include "aion/gameserver/network/aion/serverpackets/SM_CREATE_CHARACTER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CREATE_CHARACTER::SM_CREATE_CHARACTER(runtime::Ptr<model::account::PlayerAccountData> accPlData, int32_t responseCodeValue)
	: AbstractPlayerInfoPacket(opcodeOf<SM_CREATE_CHARACTER>), responseCode(responseCodeValue), playerAccData(accPlData) {
}

SM_CREATE_CHARACTER::~SM_CREATE_CHARACTER() = default;

void SM_CREATE_CHARACTER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
