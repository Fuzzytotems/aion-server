#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"

#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAY_MOVIE::SM_PLAY_MOVIE(bool isCutsceneMovie, int32_t objectIdValue, int32_t questIdValue, int32_t cutsceneIdValue, bool canSkipValue)
	: AionServerPacket(opcodeOf<SM_PLAY_MOVIE>), isMovie(isCutsceneMovie), objectId(objectIdValue), questId(questIdValue),
	  cutsceneId(cutsceneIdValue), canSkip(canSkipValue) {
}

void SM_PLAY_MOVIE::writeImpl(AionConnection* con) {
	// side effect of the serialization for each recipient (runtime-architecture.md §8.5): never cache this packet
	detail::requireConnection(con, "SM_PLAY_MOVIE").getActivePlayer()->setCustomState(model::gameobjects::player::CustomPlayerState::WATCHING_CUTSCENE);
	writeC(isMovie ? 1 : 0); // if 1: CutSceneMovies else CutScenes
	writeD(objectId);
	writeD(questId);
	writeD(cutsceneId);
	writeC(0); // unknown
	writeC(canSkip ? 0 : 1);
}

} // namespace aion::gameserver::network::aion::serverpackets
