#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SEARCH.h"

#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_SEARCH::SM_PLAYER_SEARCH(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& playersValue)
	: AionServerPacket(opcodeOf<SM_PLAYER_SEARCH>), players(playersValue.begin(), playersValue.end()) {
}

SM_PLAYER_SEARCH::~SM_PLAYER_SEARCH() = default;

void SM_PLAYER_SEARCH::writeImpl(AionConnection* con) {
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = detail::requireConnection(con, "SM_PLAYER_SEARCH").getActivePlayer();
	writeH(static_cast<int32_t>(players.size()));
	for (const runtime::Ref<model::gameobjects::player::Player>& player : players) {
		writeD(player->getWorldId());
		writeF(player->getX());
		writeF(player->getY());
		writeF(player->getZ());
		writeC(model::getClassId(player->getPlayerClass()));
		writeC(model::getGenderId(player->getGender()));
		writeC(player->getLevel());
		writeC(player->getPlayerSettings()->isInDeniedStatus(model::gameobjects::player::DeniedStatus::GROUP) ? 1
				: player->isInTeam()                                                                              ? 3
				: player->isLookingForGroup()                                                                     ? 2
																												  : 0);
		// Java: ChatUtil.toFactionPrefixedName(activePlayer, player) (NullPointerException without an active player)
		writeS(utils::ChatUtil::toFactionPrefixedName(*activePlayer, *player), AbstractPlayerInfoPacket::CHARNAME_MAX_LENGTH + 2);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
