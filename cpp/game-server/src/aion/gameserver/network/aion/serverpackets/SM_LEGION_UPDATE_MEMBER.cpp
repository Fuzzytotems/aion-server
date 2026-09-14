#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_MEMBER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_UPDATE_MEMBER::SM_LEGION_UPDATE_MEMBER(model::gameobjects::player::Player& player, int32_t msgIdValue, std::string_view textValue)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_MEMBER>) {
	AION_UNPORTED();
}

SM_LEGION_UPDATE_MEMBER::SM_LEGION_UPDATE_MEMBER(model::team::legion::LegionMember& legionMemberValue, int32_t msgIdValue, std::string_view textValue)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_MEMBER>), legionMember(legionMemberValue), msgId(msgIdValue), text(textValue) {
}

SM_LEGION_UPDATE_MEMBER::SM_LEGION_UPDATE_MEMBER(model::team::legion::LegionMember& legionMemberValue)
	: SM_LEGION_UPDATE_MEMBER(legionMemberValue, 0, std::string_view()) {
}

SM_LEGION_UPDATE_MEMBER::~SM_LEGION_UPDATE_MEMBER() = default;

void SM_LEGION_UPDATE_MEMBER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
