#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_INFO::SM_LEGION_INFO(model::team::legion::Legion& legionValue)
	: SM_LEGION_INFO(opcodeOf<SM_LEGION_INFO>, legionValue) {
}

SM_LEGION_INFO::SM_LEGION_INFO(int32_t opCode, model::team::legion::Legion& legionValue)
	: AionServerPacket(opCode), legion(legionValue) {
}

SM_LEGION_INFO::~SM_LEGION_INFO() = default;

void SM_LEGION_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_LEGION_INFO::writeAnnouncements() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
