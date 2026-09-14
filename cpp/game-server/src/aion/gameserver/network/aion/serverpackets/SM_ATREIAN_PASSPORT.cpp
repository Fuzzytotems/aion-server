#include "aion/gameserver/network/aion/serverpackets/SM_ATREIAN_PASSPORT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/account/PassportsList.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ATREIAN_PASSPORT::SM_ATREIAN_PASSPORT(model::account::PassportsList& passportsValue, int32_t stampsValue,
	commons::database::Date accountCreationDateValue)
	: AionServerPacket(opcodeOf<SM_ATREIAN_PASSPORT>), accountCreationDate(accountCreationDateValue), passports(passportsValue), stamps(stampsValue) {
}

SM_ATREIAN_PASSPORT::~SM_ATREIAN_PASSPORT() = default;

void SM_ATREIAN_PASSPORT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
