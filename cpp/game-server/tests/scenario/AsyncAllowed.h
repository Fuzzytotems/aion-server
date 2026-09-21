#pragma once

// AsyncAllowed (m5a-plan.md §5.9): the set of server packets a scenario case tolerates at any position of its PacketSequence. The plan lists
// six of them, and four are conditional, so a name alone is not enough:
//   - periodic SM_GAME_TIME (every 180 s) and SM_PONG: always allowed
//   - SM_WEATHER (a weather change 20-240 s after an in-game hour): always allowed
//   - SM_PLAYER_STATE to self only (the end of the 60 s spawn protection, PlayerController.java:626); a state of another player is not async
//   - SM_NPC_INFO / SM_DELETE of temporary-spawn object ids at an hour change, and only outside the region-move window of case 5
//   - SM_SYSTEM_MESSAGE only for STR_SERVER_SHUTDOWN, and only in case 7
// The object and message ids come from the independent decoders of F-08, never from the server's packet classes.

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aion::gameserver::scenario {

class AsyncAllowed {
public:
	/** SM_SYSTEM_MESSAGE.STR_SERVER_SHUTDOWN (SM_SYSTEM_MESSAGE.java:12303) */
	static constexpr int32_t STR_SERVER_SHUTDOWN = 1300642;

	/** The unconditional part of §5.9: SM_GAME_TIME, SM_PONG and SM_WEATHER. */
	static AsyncAllowed m5aDefault();

	/** allows SM_PLAYER_STATE whose object id is the scenario's own player */
	AsyncAllowed& selfPlayerState(int32_t objectId);

	/**
	 * allows SM_NPC_INFO and SM_DELETE of temporary-spawn object ids (an in-game hour change). The region-move window of case 5 must not pass
	 * this: there every SM_NPC_INFO and SM_DELETE is part of the expected sequence (M1, M2).
	 */
	AsyncAllowed& temporarySpawnUpdates(std::function<bool(int32_t objectId)> isTemporarySpawn);

	/** allows SM_SYSTEM_MESSAGE with STR_SERVER_SHUTDOWN (case 7 only) */
	AsyncAllowed& serverShutdownMessage();

	/** @return true if this packet may appear at any position of the sequence. A body that does not decode is never async allowed. */
	bool allows(std::string_view name, std::span<const uint8_t> body) const;

	/** the names this set allows unconditionally, for a failure message */
	const std::vector<std::string>& unconditionalNames() const noexcept { return names; }

	/**
	 * The index predicate PacketSequence::match takes. `packets` must outlive the returned function; each element needs a `name` (convertible
	 * to std::string_view) and a `data` member (a byte range), as GameSession::Packet has.
	 */
	template <typename Packets>
	std::function<bool(size_t)> predicate(const Packets& packets) const {
		return [this, &packets](size_t index) {
			const auto& packet = packets[index];
			return allows(packet.name, std::span<const uint8_t>(packet.data));
		};
	}

private:
	std::vector<std::string> names;
	int32_t selfObjectId = 0;
	bool selfPlayerStateAllowed = false;
	std::function<bool(int32_t)> temporarySpawn;
	bool shutdownMessageAllowed = false;
};

} // namespace aion::gameserver::scenario
