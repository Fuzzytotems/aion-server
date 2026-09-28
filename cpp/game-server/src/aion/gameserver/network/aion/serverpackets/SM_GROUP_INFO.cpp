#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_GROUP_INFO::SM_GROUP_INFO(model::team::group::PlayerGroup& group)
	: AionServerPacket(opcodeOf<SM_GROUP_INFO>), lootRules(group.getLootGroupRules()), groupId(group.getObjectId()),
	  // C++: GeneralTeam::getLeader returns the same member as Java's PlayerGroup.getLeader() cast (PlayerGroupMember.h is not written yet, P5-10)
	  leaderId(static_cast<model::team::GeneralTeam&>(group).getLeader()->getObjectId()), type(group.getTeamType()) {
}

SM_GROUP_INFO::~SM_GROUP_INFO() = default;

void SM_GROUP_INFO::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_GROUP_INFO::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	writeD(groupId);
	writeD(leaderId);
	writeD(!player || !player->getPosition() ? 0 : player->getWorldId()); // mapId
	writeD(detail::lootRuleId(lootRules->getLootRule()));
	writeD(lootRules->getMisc());
	writeD(lootRules->getCommonItemAbove());
	writeD(lootRules->getSuperiorItemAbove());
	writeD(lootRules->getHeroicItemAbove());
	writeD(lootRules->getFabledItemAbove());
	writeD(lootRules->getEternalItemAbove());
	writeD(lootRules->getMythicItemAbove());
	writeD(0x02);
	writeC(0x00);
	writeD(detail::teamTypeType(type));
	writeD(detail::teamTypeSubType(type));
	writeD(0x00); // message id
	writeS(""); // name
}

} // namespace aion::gameserver::network::aion::serverpackets
