#include "aion/gameserver/handlers/admincommands/Movie.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Movie);

Movie::Movie()
	: AdminCommand("movie", "Plays movies/cutscenes.",
		  "<cutscene ID> - Plays the given cutscene (correct rendering depends on your current map).\n"
		  "m <movie ID> - Plays the given movie cutscene.\n") {
}

// Java Movie.java:21-30
void Movie::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}
	bool isCutsceneMovie = commons::utils::StringUtils::equalsIgnoreCase("m", params[0]);
	if (params.size() < (isCutsceneMovie ? 2u : 1u)) // parity: Java's ArrayIndexOutOfBoundsException of params[1] ("//movie m"), explicit
		throw runtime::ArrayIndexOutOfBoundsException("Index 1 out of bounds for length 1"); // parity: (the same; ChatCommand.run logs it)
	int32_t cutsceneId = commons::utils::parseInt(params[isCutsceneMovie ? 1 : 0]);
	PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(isCutsceneMovie, 0, 0, cutsceneId, true));
}

} // namespace aion::gameserver::handlers::admincommands
