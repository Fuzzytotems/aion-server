#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_EMBLEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_UPDATE_EMBLEM::SM_LEGION_UPDATE_EMBLEM(int32_t legionIdValue, model::team::legion::LegionEmblem& emblem)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_EMBLEM>), legionId(legionIdValue) {
	AION_UNPORTED();
}

void SM_LEGION_UPDATE_EMBLEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
