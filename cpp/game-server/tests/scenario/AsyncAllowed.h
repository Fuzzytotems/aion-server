#pragma once

// AsyncAllowed (m5a-plan.md §5.9): the set of server packets a scenario case tolerates at any position of its PacketSequence. The plan lists
// six of them, and four are conditional, so a name alone is not enough:
//   - periodic SM_GAME_TIME (every 180 s) and SM_PONG: always allowed
//   - SM_WEATHER (a weather change 20-240 s after an in-game hour): always allowed
//   - SM_PLAYER_STATE to self only (the end of the 60 s spawn protection, PlayerController.java:626); a state of another player is not async
//   - SM_NPC_INFO / SM_DELETE of temporary-spawn object ids at an hour change, and only outside the region-move window of case 5
//   - SM_SYSTEM_MESSAGE only for STR_SERVER_SHUTDOWN, and only in case 7
//   - SM_MOVE, SM_EMOTION, SM_LOOKATOBJECT, SM_ATTACK and SM_ATTACK_STATUS whose every object id is an npc the server already announced
//     (m5b-plan.md D2 and G-05, added when the three root AI handlers were registered): npcs walk now, and npcs of hostile tribes fight each
//     other. The character's own object id is never one of them, so a packet about the character is never async - which is what M3 asserts
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

	/**
	 * m5b-plan.md D2 and G-05: with `general`, `aggressive` and `noaction` registered, every npc of an active map region thinks, and what it
	 * then does reaches every client that knows it. **Measured** in the two gates of this file, on both start maps:
	 *  - a path walker leaves its spot the moment the character's region goes active (ThinkEventHandler::thinkIdle ->
	 *    WalkManager::startWalking) and a random walker 3 to 15 s later (AIConfig MINIMIMUM_DELAY / MAXIMUM_DELAY). Each broadcasts the
	 *    EmoteManager emote of what it started and one SM_MOVE per move start.
	 *  - npcs of hostile tribes fight **each other**. That is Java, not a port defect: CreatureEventHandler.checkAggro takes any `Creature`
	 *    and decides by `TribeRelationService.isAggressive` and `Creature.isEnemyFrom` (CreatureEventHandler.java:56-95), so a guard that
	 *    sees a monster aggroes it with no player involved. On 220010000 such a fight runs inside the Mage's level-ready window and sends
	 *    SM_LOOKATOBJECT, the attack emotes, SM_ATTACK and SM_ATTACK_STATUS.
	 * None of it belongs to a position of §5.8 and no configuration turns a registered handler off, so all of it is async from case 4 on.
	 *
	 * **What keeps this from being a hole: every object id the packet names must be an announced npc.** The character's own object id is
	 * never announced by an SM_NPC_INFO - it arrives in SM_PLAYER_INFO and SM_PLAYER_SPAWN - so the character cannot appear in any packet
	 * this set allows, and a gate whose character is moved, looked at, attacked or damaged still fails at that packet's position. Per packet:
	 *  - SM_MOVE: the mover is an announced npc. The character's own SM_MOVE stays a failure, which is §5.6 M3.
	 *  - SM_EMOTION: the sender is an announced npc **and** the emotion is one of EmoteManager's four (decoders::isNpcEmote). A death, a
	 *    resurrection, a loot or a chair emote is not one of them and remains news; the body of an allowed one is consumed exactly, so a
	 *    port that changes SM_EMOTION's framing fails here too.
	 *  - SM_LOOKATOBJECT: the looker is an announced npc and it looks at an announced npc or at nothing (target id 0). The whole 9-byte body
	 *    is consumed.
	 *  - SM_ATTACK: attacker **and** target are announced npcs.
	 *  - SM_ATTACK_STATUS: the creature whose HP or MP changed is an announced npc.
	 *
	 * The order the predicate relies on: an npc is announced before it can do any of this, because the server broadcasts only to players
	 * that know it and a player knows it by having been sent the SM_NPC_INFO first, on this same connection.
	 */
	AsyncAllowed& npcActivity(std::function<bool(int32_t objectId)> isAnnouncedNpc);

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
	std::function<bool(int32_t)> announcedNpc;
};

} // namespace aion::gameserver::scenario
