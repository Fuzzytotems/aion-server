#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_MEMBER.h"

#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_UPDATE_MEMBER::SM_LEGION_UPDATE_MEMBER(model::gameobjects::player::Player& player, int32_t msgIdValue, std::string_view textValue)
	: SM_LEGION_UPDATE_MEMBER(*player.getLegionMember(), msgIdValue, textValue) {
}

SM_LEGION_UPDATE_MEMBER::SM_LEGION_UPDATE_MEMBER(model::team::legion::LegionMember& legionMemberValue, int32_t msgIdValue, std::string_view textValue)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_MEMBER>), legionMember(legionMemberValue), msgId(msgIdValue), text(textValue) {
}

SM_LEGION_UPDATE_MEMBER::SM_LEGION_UPDATE_MEMBER(model::team::legion::LegionMember& legionMemberValue)
	: SM_LEGION_UPDATE_MEMBER(legionMemberValue, 0, std::string_view()) {
}

SM_LEGION_UPDATE_MEMBER::~SM_LEGION_UPDATE_MEMBER() = default;

void SM_LEGION_UPDATE_MEMBER::writeImpl(AionConnection* con) {
	writeD(legionMember->getObjectId());
	writeC(detail::legionRankId(legionMember->getRank()));
	writeC(model::getClassId(legionMember->getPlayerClass()));
	writeC(legionMember->getLevel());
	writeD(legionMember->getWorldId());
	writeC(legionMember->isOnline() ? 1 : 0);
	writeD(legionMember->isOnline() ? 0 : legionMember->getLastOnlineEpochSeconds());
	writeD(configs::network::NetworkConfig::GAMESERVER_ID.load()); // TODO: add to account model?
	writeD(msgId);
	writeS(text); // Java null (one-argument constructor): writeS writes only the terminator, like ""
}

} // namespace aion::gameserver::network::aion::serverpackets
