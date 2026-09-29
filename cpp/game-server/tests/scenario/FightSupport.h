#pragma once

// The kill helpers of the fight gates (m5d-plan.md G-02), lifted out of the gate files that wrote them so that the M5d gate's kerub and sprigg
// worker kills (§10.2 C10, C12, C16) use the same code as M5b's and M5b-2's:
// - FightRecording and recordFight, from M5bScenarioTest.cpp (m5b-plan.md §6.3: one decoded pass over a fight);
// - waitForRespawnAt, from M5b2ScenarioTest.cpp (m5b3-plan.md §18.1, G-07), where it was a lambda over the gate's client A.
// The code is moved unchanged. The one difference is that waitForRespawnAt takes the session and the npc template as parameters instead of
// capturing the gate's client and defaulting to its monster.
//
// Everything is read with the independent decoders of decoders/ (m5a-plan.md D9); nothing here includes a C++ serverpackets header.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "GameSession.h"
#include "Oracle.h" // OracleMonsterSpot
#include "decoders/CombatDecoders.h"

namespace aion::gameserver::scenario {

/**
 * One pass over a slice of the session's recording, decoded with the independent combat decoders. Every §6.3 attack assertion reads this
 * structure instead of the raw packets, so that the decode happens once and a body that does not decode is a failure of the case that
 * collected it rather than of the assertion that happens to look first.
 */
struct FightRecording {
	struct AttackPacket {
		size_t index = 0;
		decoders::Attack attack;
		int32_t totalDamage = 0;
		std::chrono::steady_clock::time_point at;
	};
	struct StatusPacket {
		size_t index = 0;
		decoders::AttackStatusUpdate status;
		std::chrono::steady_clock::time_point at;
	};
	struct HpPacket {
		size_t index = 0;
		decoders::StatUpdateHp hp;
		std::chrono::steady_clock::time_point at;
	};

	std::vector<AttackPacket> attacks;
	std::vector<StatusPacket> statuses;
	std::vector<HpPacket> hpUpdates;
	std::vector<std::pair<size_t, decoders::AttackResponse>> responses;
	std::vector<std::pair<size_t, decoders::Emotion>> emotions;
	/** the decode failures, so a §6.3 assertion never silently sees a shorter stream than the run produced */
	std::vector<std::string> decodeFailures;

	std::vector<AttackPacket> attacksBy(int32_t attackerObjectId) const;
	std::vector<AttackPacket> attacksBetween(int32_t attackerObjectId, int32_t targetObjectId) const;
	std::vector<StatusPacket> statusesOf(int32_t creatureObjectId) const;
};

/**
 * Decodes the packets [from, end) of the session's recording into a FightRecording: SM_ATTACK (with the sum of its results' damage),
 * SM_ATTACK_STATUS, SM_STATUPDATE_HP, SM_ATTACK_RESPONSE and SM_EMOTION. Every other packet is skipped; a body that does not decode goes to
 * decodeFailures with its name and index.
 */
FightRecording recordFight(const GameSession& session, size_t from);

/**
 * G-07 (m5b3-plan.md §18.1): the respawn of the npc of `templateId` that died on `spot` at recording index `diedAt` - the object id of the
 * first SM_NPC_INFO at the spot (within 0.01 on each axis) recorded after the death whose object was never announced at the spot before it.
 * A respawn is a new object and cannot be announced before the death; every object announced there before it is the dead one or an npc an
 * earlier case killed (a corpse that comes back into view is announced again under its own id). "The latest one at the spot but the dead
 * one" is not enough here: before the respawn is announced it answers the npc an earlier case killed at the same spot (the M5b-2 review's
 * mutant RS10b pulled S5's corpse of monster A).
 *
 * The packets already recorded from `diedAt` on are searched first; after them the call reads (and records) until such a packet arrives or
 * `timeout` passes. A packet that does not decode is not the npc.
 * @return the respawned object's id, std::nullopt on timeout or a closed connection
 */
std::optional<int32_t> waitForRespawnAt(GameSession& session, const OracleMonsterSpot& spot, int32_t templateId, size_t diedAt,
	std::chrono::milliseconds timeout);

} // namespace aion::gameserver::scenario
