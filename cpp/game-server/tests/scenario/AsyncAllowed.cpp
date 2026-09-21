#include "AsyncAllowed.h"

#include <algorithm>
#include <utility>

#include "decoders/PacketDecoders.h"

namespace aion::gameserver::scenario {

AsyncAllowed AsyncAllowed::m5aDefault() {
	AsyncAllowed allowed;
	allowed.names = {"SM_GAME_TIME", "SM_PONG", "SM_WEATHER"};
	return allowed;
}

AsyncAllowed& AsyncAllowed::selfPlayerState(int32_t objectId) {
	selfObjectId = objectId;
	selfPlayerStateAllowed = true;
	return *this;
}

AsyncAllowed& AsyncAllowed::temporarySpawnUpdates(std::function<bool(int32_t)> isTemporarySpawn) {
	temporarySpawn = std::move(isTemporarySpawn);
	return *this;
}

AsyncAllowed& AsyncAllowed::serverShutdownMessage() {
	shutdownMessageAllowed = true;
	return *this;
}

bool AsyncAllowed::allows(std::string_view name, std::span<const uint8_t> body) const {
	if (std::ranges::find(names, name) != names.end())
		return true;
	try {
		if (selfPlayerStateAllowed && name == "SM_PLAYER_STATE")
			return decoders::decodePlayerStateObjectId(body) == selfObjectId;
		if (temporarySpawn && name == "SM_NPC_INFO")
			return temporarySpawn(decoders::decodeNpcInfoObjectId(body));
		if (temporarySpawn && name == "SM_DELETE")
			return temporarySpawn(decoders::decodeDeleteObjectId(body));
		if (shutdownMessageAllowed && name == "SM_SYSTEM_MESSAGE")
			return decoders::decodeSystemMessageId(body) == STR_SERVER_SHUTDOWN;
	} catch (const decoders::DecodeError&) {
		// a body that does not decode is a gate failure, not an async packet: let the sequence match it (and fail) at its position
		return false;
	}
	return false;
}

} // namespace aion::gameserver::scenario
