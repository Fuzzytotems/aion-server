#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_EDIT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_EDIT::SM_LEGION_EDIT(int32_t typeValue)
	: AionServerPacket(opcodeOf<SM_LEGION_EDIT>), type(typeValue) {
}

SM_LEGION_EDIT::SM_LEGION_EDIT(int32_t typeValue, model::team::legion::Legion& legionValue)
	: AionServerPacket(opcodeOf<SM_LEGION_EDIT>), type(typeValue), legion(legionValue) {
}

SM_LEGION_EDIT::SM_LEGION_EDIT(int32_t typeValue, int32_t unixTimeValue)
	: AionServerPacket(opcodeOf<SM_LEGION_EDIT>), type(typeValue), unixTime(unixTimeValue) {
}

SM_LEGION_EDIT::SM_LEGION_EDIT(model::team::legion::Legion::Announcement& announcementValue)
	: AionServerPacket(opcodeOf<SM_LEGION_EDIT>), type(0x05) {
	AION_UNPORTED();
}

SM_LEGION_EDIT::~SM_LEGION_EDIT() = default;

void SM_LEGION_EDIT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
