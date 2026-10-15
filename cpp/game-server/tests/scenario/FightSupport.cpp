#include "FightSupport.h"

#include <cmath>
#include <set>

#include "decoders/PacketDecoders.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {

using decoders::DecodeError;
using Packet = GameSession::Packet;

// ---- the fight recording (from M5bScenarioTest.cpp) -------------------------------------------------------------------------------------

std::vector<FightRecording::AttackPacket> FightRecording::attacksBy(int32_t attackerObjectId) const {
	std::vector<AttackPacket> result;
	for (const AttackPacket& attack : attacks)
		if (attack.attack.attackerObjectId == attackerObjectId)
			result.push_back(attack);
	return result;
}

std::vector<FightRecording::AttackPacket> FightRecording::attacksBetween(int32_t attackerObjectId, int32_t targetObjectId) const {
	std::vector<AttackPacket> result;
	for (const AttackPacket& attack : attacks)
		if (attack.attack.attackerObjectId == attackerObjectId && attack.attack.targetObjectId == targetObjectId)
			result.push_back(attack);
	return result;
}

std::vector<FightRecording::StatusPacket> FightRecording::statusesOf(int32_t creatureObjectId) const {
	std::vector<StatusPacket> result;
	for (const StatusPacket& status : statuses)
		if (status.status.creatureObjectId == creatureObjectId)
			result.push_back(status);
	return result;
}

FightRecording recordFight(const GameSession& session, size_t from) {
	FightRecording recording;
	const std::vector<Packet>& packets = session.recorded();
	for (size_t i = from; i < packets.size(); i++) {
		const Packet& packet = packets[i];
		try {
			if (packet.name == "SM_ATTACK") {
				FightRecording::AttackPacket attack;
				attack.index = i;
				attack.at = packet.receivedAt;
				attack.attack = decoders::decodeAttack(packet.data);
				for (const decoders::AttackResultEntry& entry : attack.attack.results)
					attack.totalDamage += entry.damage;
				recording.attacks.push_back(attack);
			} else if (packet.name == "SM_ATTACK_STATUS") {
				recording.statuses.push_back({i, decoders::decodeAttackStatus(packet.data), packet.receivedAt});
			} else if (packet.name == "SM_STATUPDATE_HP") {
				recording.hpUpdates.push_back({i, decoders::decodeStatUpdateHp(packet.data), packet.receivedAt});
			} else if (packet.name == "SM_ATTACK_RESPONSE") {
				recording.responses.emplace_back(i, decoders::decodeAttackResponse(packet.data));
			} else if (packet.name == "SM_EMOTION") {
				recording.emotions.emplace_back(i, decoders::decodeEmotion(packet.data));
			}
		} catch (const DecodeError& error) {
			recording.decodeFailures.push_back(packet.name + " at " + std::to_string(i) + ": " + error.what());
		}
	}
	return recording;
}

// ---- the respawn (from M5b2ScenarioTest.cpp) --------------------------------------------------------------------------------------------

namespace {

/**
 * Reads until `accept` answers true for a packet, recording everything on the way, or until `timeout`. The predicate sees every packet once.
 * M5b2ScenarioTest.cpp's readUntil, which the lambda called.
 * @return the index in recorded() of the accepted packet, std::nullopt on timeout or close
 */
template <typename Accept>
std::optional<size_t> readUntil(GameSession& session, const Accept& accept, std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			return std::nullopt;
		std::optional<Packet> packet = session.next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet) {
			if (session.client.socket.isClosed())
				return std::nullopt;
			continue;
		}
		if (accept(*packet))
			return session.recorded().size() - 1;
	}
}

} // namespace

std::optional<int32_t> waitForRespawnAt(GameSession& session, const OracleMonsterSpot& spot, int32_t templateId, size_t diedAt,
	std::chrono::milliseconds timeout) {
	const auto atSpot = [&](const Packet& packet) -> std::optional<int32_t> {
		if (packet.name != "SM_NPC_INFO")
			return std::nullopt;
		try {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			if (npc.templateId == templateId && std::abs(npc.x - spot.x) <= 0.01f && std::abs(npc.y - spot.y) <= 0.01f &&
			    std::abs(npc.z - spot.z) <= 0.01f)
				return npc.objectId;
		} catch (const DecodeError&) {
			// a packet that does not decode is not this npc
		}
		return std::nullopt;
	};
	std::set<int32_t> announcedBefore;
	for (size_t i = 0; i < diedAt && i < session.recorded().size(); i++)
		if (const std::optional<int32_t> id = atSpot(session.recorded()[i]))
			announcedBefore.insert(*id);
	for (size_t i = diedAt; i < session.recorded().size(); i++)
		if (const std::optional<int32_t> id = atSpot(session.recorded()[i]); id && !announcedBefore.contains(*id))
			return id;
	const std::optional<size_t> index = readUntil(
	  session,
	  [&](const Packet& packet) {
		  const std::optional<int32_t> id = atSpot(packet);
		  return id && !announcedBefore.contains(*id);
	  },
	  timeout);
	if (!index)
		return std::nullopt;
	return atSpot(session.recorded()[*index]);
}

} // namespace aion::gameserver::scenario
