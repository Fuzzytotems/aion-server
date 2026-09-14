#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GROUP_INFO::SM_GROUP_INFO(model::team::group::PlayerGroup& group) : AionServerPacket(opcodeOf<SM_GROUP_INFO>) {
	AION_UNPORTED();
}

SM_GROUP_INFO::~SM_GROUP_INFO() = default;

void SM_GROUP_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
