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
	const size_t index = isCutsceneMovie ? 1 : 0;
	if (index >= params.size()) // Java: params[1] of "//movie m" throws ArrayIndexOutOfBoundsException (ChatCommand.run logs it)
		throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(params.size()));
	int32_t cutsceneId = commons::utils::parseInt(params[index]);
	PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(isCutsceneMovie, 0, 0, cutsceneId, true));
}

} // namespace aion::gameserver::handlers::admincommands
