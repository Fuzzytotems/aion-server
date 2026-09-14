#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SUMMON_UPDATE::SM_SUMMON_UPDATE(model::gameobjects::Summon& summonValue)
	: AionServerPacket(opcodeOf<SM_SUMMON_UPDATE>), summon(summonValue) {
}

SM_SUMMON_UPDATE::~SM_SUMMON_UPDATE() = default;

void SM_SUMMON_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
