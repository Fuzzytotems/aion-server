#include "aion/gameserver/network/aion/clientpackets/CM_PLAY_MOVIE_END.h"

#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::CustomPlayerState;

CM_PLAY_MOVIE_END::CM_PLAY_MOVIE_END(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_PLAY_MOVIE_END.java:33-41
void CM_PLAY_MOVIE_END::readImpl() {
	type = readC(); // 1: CutSceneMovies, otherwise CutScenes
	targetObjectId = readD();
	questId = readD();
	movieId = readD();
	readC(); // unknown
	canSkip = readC() == 0;
}

// Java CM_PLAY_MOVIE_END.java:43-56
void CM_PLAY_MOVIE_END::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player->isInCustomState(CustomPlayerState::WATCHING_CUTSCENE)) {
		// the client automatically plays movies when reading certain books (3: 730079/730091, 4: 730092, 5: 730085)
		// Java: Set<Integer> bookMovieIds = type == 1 ? Set.of(3, 4, 5) : Set.of();
		const bool bookMovie = type == 1 && (movieId == 3 || movieId == 4 || movieId == 5);
		if (questId != 0 || !bookMovie)
			utils::audit::AuditLogger::log(*player,
				"sent " + getPacketName() + " for cutscene " + std::to_string(movieId) + " that wasn't sent by the server");
		return;
	}
	player->unsetCustomState(CustomPlayerState::WATCHING_CUTSCENE);
	runtime::Ptr<model::gameobjects::VisibleObject> target = player->isTargeting(targetObjectId) ? player->getTarget() : nullptr;
	questEngine::QuestEngine::getInstance().onMovieEnd(*questEngine::model::QuestEnv::create(target, *player, questId), movieId);
	player->getPosition()->getWorldMapInstance()->getInstanceHandler()->onPlayMovieEnd(*player, movieId);
}

AION_CLIENT_PACKET(CM_PLAY_MOVIE_END);

} // namespace aion::gameserver::network::aion::clientpackets
