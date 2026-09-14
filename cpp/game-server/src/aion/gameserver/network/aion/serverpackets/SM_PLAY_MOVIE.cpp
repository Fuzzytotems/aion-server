#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAY_MOVIE::SM_PLAY_MOVIE(bool isCutsceneMovie, int32_t objectIdValue, int32_t questIdValue, int32_t cutsceneIdValue, bool canSkipValue)
	: AionServerPacket(opcodeOf<SM_PLAY_MOVIE>), isMovie(isCutsceneMovie), objectId(objectIdValue), questId(questIdValue),
	  cutsceneId(cutsceneIdValue), canSkip(canSkipValue) {
}

void SM_PLAY_MOVIE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
