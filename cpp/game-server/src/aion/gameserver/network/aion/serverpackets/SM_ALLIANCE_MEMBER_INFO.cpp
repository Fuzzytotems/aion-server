#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ALLIANCE_MEMBER_INFO::SM_ALLIANCE_MEMBER_INFO(model::team::alliance::PlayerAllianceMember& member,
	model::team::common::legacy::PlayerAllianceEvent eventValue, int32_t slotValue)
	: AionServerPacket(opcodeOf<SM_ALLIANCE_MEMBER_INFO>) {
	AION_UNPORTED();
}

SM_ALLIANCE_MEMBER_INFO::SM_ALLIANCE_MEMBER_INFO(model::team::alliance::PlayerAllianceMember& member,
	model::team::common::legacy::PlayerAllianceEvent eventValue)
	: SM_ALLIANCE_MEMBER_INFO(member, eventValue, 0) {
}

SM_ALLIANCE_MEMBER_INFO::~SM_ALLIANCE_MEMBER_INFO() = default;

void SM_ALLIANCE_MEMBER_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
