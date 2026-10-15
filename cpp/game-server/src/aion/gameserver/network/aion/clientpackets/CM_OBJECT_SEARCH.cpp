#include "aion/gameserver/network/aion/clientpackets/CM_OBJECT_SEARCH.h"

#include <optional>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/spawns/SpawnSearchResult.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_NPC_ON_MAP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using model::templates::spawns::SpawnSearchResult;

CM_OBJECT_SEARCH::CM_OBJECT_SEARCH(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_OBJECT_SEARCH.java:29-32
void CM_OBJECT_SEARCH::readImpl() {
	this->npcId = readD();
}

// Java CM_OBJECT_SEARCH.java:34-46
void CM_OBJECT_SEARCH::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
	if (!activePlayer) {
		return;
	}
	std::optional<SpawnSearchResult> searchResult = dataholders::DataManager::SPAWNS_DATA->getNearestSpawnByNpcId(activePlayer, npcId, activePlayer->getWorldId());
	if (searchResult)
		sendPacket(serverpackets::SM_SHOW_NPC_ON_MAP(*activePlayer, npcId, searchResult->getWorldId(), searchResult->getSpot().getX(), searchResult->getSpot().getY(),
			searchResult->getSpot().getZ()));
	else
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_FIND_POS_UNKNOWN_NAME());
}

AION_CLIENT_PACKET(CM_OBJECT_SEARCH);

} // namespace aion::gameserver::network::aion::clientpackets
