#include "aion/gameserver/network/aion/clientpackets/CM_PLAYER_SEARCH.h"

#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SEARCH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using configs::main::CustomConfig;
using model::gameobjects::player::FriendList;
using model::gameobjects::player::Player;

CM_PLAYER_SEARCH::CM_PLAYER_SEARCH(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_PLAYER_SEARCH.java:41-49
void CM_PLAYER_SEARCH::readImpl() {
	name = utils::Util::convertName(readS(25));
	region = readD();
	classMask = readD();
	minLevel = readUC();
	maxLevel = readUC();
	lfgOnly = readUC();
	readC(); // 0x00 in search pane 0x30 in /who?
}

// Java CM_PLAYER_SEARCH.java:52-92
void CM_PLAYER_SEARCH::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();

	if (activePlayer->getLevel() < CustomConfig::LEVEL_TO_SEARCH.load()) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_CANT_WHO_LEVEL(CustomConfig::LEVEL_TO_SEARCH.load()));
		return;
	}

	const std::string lowerName = commons::utils::StringUtils::toLowerCase(name);
	std::vector<runtime::Ptr<Player>> matches;
	for (const runtime::Ptr<Player>& player : world::World::getInstance().getAllPlayers()) {
		if (!activePlayer->isStaff()) { // staff can find all players
			if (player->getRace() != activePlayer->getRace() && !CustomConfig::FACTIONS_SEARCH_MODE.load())
				continue;
			if (player->getFriendList().getStatus() == FriendList::Status::OFFLINE)
				continue;
			if (player->isStaff() && !CustomConfig::SEARCH_GM_LIST.load())
				continue;
		}
		if (lfgOnly == 1 && !player->isLookingForGroup())
			continue;
		if (!name.empty() && commons::utils::StringUtils::toLowerCase(player->getName()).find(lowerName) == std::string::npos)
			continue;
		if (minLevel != 0xFF && player->getLevel() < minLevel)
			continue;
		if (maxLevel != 0xFF && player->getLevel() > maxLevel)
			continue;
		if (classMask > 0 && ((1 << model::getClassId(player->getPlayerClass())) & classMask) == 0)
			continue;
		if (region > 0 && player->getWorldId() != region)
			continue;
		if (player.rawPointer() == activePlayer.rawPointer())
			continue;

		matches.push_back(player);

		if (static_cast<int32_t>(matches.size()) == MAX_RESULTS)
			break;
	}

	sendPacket(serverpackets::SM_PLAYER_SEARCH(matches));
}

AION_CLIENT_PACKET(CM_PLAYER_SEARCH);

} // namespace aion::gameserver::network::aion::clientpackets
