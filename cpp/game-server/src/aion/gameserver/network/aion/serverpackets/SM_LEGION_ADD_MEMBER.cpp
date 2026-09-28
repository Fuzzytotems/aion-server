#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_ADD_MEMBER.h"

#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_ADD_MEMBER::SM_LEGION_ADD_MEMBER(model::gameobjects::player::Player& playerValue, bool isMemberValue, int32_t msgIdValue,
	std::string_view textValue)
	: AionServerPacket(opcodeOf<SM_LEGION_ADD_MEMBER>), player(playerValue), isMember(isMemberValue), msgId(msgIdValue), text(textValue) {
}

SM_LEGION_ADD_MEMBER::~SM_LEGION_ADD_MEMBER() = default;

void SM_LEGION_ADD_MEMBER::writeImpl(AionConnection* con) {
	writeD(player->getObjectId());
	writeS(player->getName());
	writeC(detail::legionRankId(player->getLegionMember()->getRank()));
	writeC(isMember ? 0x01 : 0x00); // is New Member?
	writeC(model::getClassId(player->getCommonData()->getPlayerClass()));
	writeC(player->getLevel());
	writeD(player->getPosition()->getMapId());
	writeD(configs::network::NetworkConfig::GAMESERVER_ID.load()); // TODO: add to account model?
	writeD(msgId);
	writeS(text);
}

} // namespace aion::gameserver::network::aion::serverpackets
