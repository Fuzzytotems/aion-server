#include "GameSession.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "aion/clientprotocol/decoders/SkillDecoders.h"
#include "ScenarioDecoders.h"

#include "aion/clientprotocol/ClientOpcodes.gen.h"

namespace aion::gameserver::scenario {

using network::test::PacketWriter;

GameSession::GameSession(uint16_t port) : client(port) {
}

std::string GameSession::nameOf(int32_t opcode) {
	if (const auto* entry = clientprotocol::ClientOpcodes::findServerPacket(opcode))
		return std::string(entry->name);
	return "SM_UNKNOWN_" + std::to_string(opcode);
}

GameSession::Packet GameSession::record(const network::test::FakeGameClient::ServerPacket& serverPacket) {
	Packet packet;
	packet.opcode = serverPacket.opcode;
	packet.name = nameOf(serverPacket.opcode);
	packet.data = serverPacket.data;
	packet.receivedAt = std::chrono::steady_clock::now();
	packets.push_back(packet);
	return packet;
}

int32_t GameSession::readKey(std::chrono::milliseconds timeout) {
	int32_t key = client.readKey(timeout);
	network::test::FakeGameClient::ServerPacket packet;
	packet.opcode = network::test::FakeGameClientCrypto::SM_KEY_OPCODE;
	packet.data = PacketWriter().D(key).data;
	record(packet);
	return key;
}

void GameSession::send(int32_t opcode, std::span<const uint8_t> data) {
	client.sendPacket(opcode, data);
}

std::optional<GameSession::Packet> GameSession::next(std::chrono::milliseconds timeout) {
	std::optional<network::test::FakeGameClient::ServerPacket> packet = client.readPacket(timeout);
	if (!packet)
		return std::nullopt;
	return record(*packet);
}

GameSession::Packet GameSession::expect(std::string_view name, std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			throw std::runtime_error("timeout waiting for " + std::string(name));
		std::optional<Packet> packet = next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet)
			throw std::runtime_error("no " + std::string(name) + " (" + (client.socket.isClosed() ? "connection closed" : "timeout") + ")");
		if (packet->name == name)
			return *packet;
	}
}

std::vector<GameSession::Packet> GameSession::collectUntilQuiet(std::chrono::milliseconds quiet, std::chrono::milliseconds limit) {
	std::vector<Packet> collected;
	const auto deadline = std::chrono::steady_clock::now() + limit;
	while (std::chrono::steady_clock::now() < deadline) {
		std::optional<Packet> packet = next(quiet);
		if (!packet)
			break;
		collected.push_back(std::move(*packet));
	}
	return collected;
}

std::vector<std::string> GameSession::names(size_t from) const {
	std::vector<std::string> result;
	for (size_t i = from; i < packets.size(); i++)
		result.push_back(packets[i].name);
	return result;
}

bool GameSession::waitClosed(std::chrono::milliseconds timeout) {
	return client.socket.waitClosed(timeout);
}

GameSession::CastOutcome GameSession::castAndWait(int32_t casterObjectId, const CastRequest& request, std::chrono::milliseconds timeout,
	const std::optional<CastInterruption>& interruption) {
	CastOutcome outcome;
	outcome.firstPacket = packets.size();
	send(CM_CASTSPELL, buildCM_CASTSPELL(request));
	outcome.sentAt = std::chrono::steady_clock::now();
	const auto deadline = outcome.sentAt + timeout;
	for (;;) {
		auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			break;
		auto until = deadline;
		if (interruption && !outcome.interruptionSentAt) {
			const auto due = outcome.sentAt + interruption->after;
			if (now >= due) {
				send(interruption->opcode, interruption->body);
				outcome.interruptionSentAt = std::chrono::steady_clock::now();
				continue;
			}
			until = std::min(until, due);
		}
		std::optional<Packet> packet = next(std::chrono::ceil<std::chrono::milliseconds>(until - now));
		if (!packet) {
			if (client.socket.isClosed()) { // readPacket answers nothing for a timeout and for a closed connection alike
				outcome.closed = true;
				break;
			}
			continue;
		}
		const size_t index = packets.size() - 1;
		if (packet->name == "SM_CASTSPELL") {
			const decoders::CastSpell cast = decoders::decodeCastSpell(packet->data);
			if (!outcome.castSpell && cast.effectorObjectId == casterObjectId && cast.spellId == request.spellId)
				outcome.castSpell = index;
		} else if (packet->name == "SM_CASTSPELL_RESULT") {
			const decoders::CastSpellResult result = decoders::decodeCastSpellResult(packet->data);
			if (result.effectorObjectId == casterObjectId && result.skillId == request.spellId) {
				outcome.castSpellResult = index;
				break;
			}
		} else if (packet->name == "SM_SKILL_CANCEL") {
			const decoders::SkillCancel cancel = decoders::decodeSkillCancel(packet->data);
			if (cancel.creatureObjectId == casterObjectId && cancel.skillId == request.spellId) {
				outcome.skillCancel = index;
				break;
			}
		}
	}
	outcome.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - outcome.sentAt);
	return outcome;
}

GameSession::FightOutcome GameSession::fightUntil(int32_t targetObjectId, std::chrono::milliseconds attackSpeed, const FightPredicate& done,
	std::chrono::milliseconds timeout, int32_t maxAttacks, uint8_t attackType) {
	FightOutcome outcome;
	outcome.firstPacket = packets.size();
	const auto start = std::chrono::steady_clock::now();
	const auto deadline = start + timeout;
	auto nextAttack = start; // the first attack goes out at once
	for (;;) {
		auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			break;
		if (now >= nextAttack) {
			if (outcome.attacksSent >= maxAttacks)
				break; // the last attack has had its interval to be answered in
			send(CM_ATTACK, buildCM_ATTACK(targetObjectId, static_cast<uint8_t>(outcome.attacksSent), 0, attackType));
			outcome.attacksSent++;
			now = std::chrono::steady_clock::now();
			nextAttack = now + attackSpeed;
		}
		const auto until = nextAttack < deadline ? nextAttack : deadline;
		if (now >= until)
			continue;
		std::optional<Packet> packet = next(std::chrono::duration_cast<std::chrono::milliseconds>(until - now));
		if (!packet) {
			if (client.socket.isClosed()) { // readPacket answers nothing for a timeout and for a closed connection alike
				outcome.closed = true;
				break;
			}
			continue; // nothing arrived before the next attack was due
		}
		if (done(*packet)) {
			outcome.done = true;
			break;
		}
	}
	outcome.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
	return outcome;
}

GameSession::TalkOutcome GameSession::talk(int32_t npcObjectId, uint16_t dialogActionId, int32_t questId, std::chrono::milliseconds quiet,
	std::chrono::milliseconds limit) {
	TalkOutcome outcome;
	outcome.firstPacket = packets.size();
	send(CM_DIALOG_SELECT, buildCM_DIALOG_SELECT(npcObjectId, dialogActionId, 0, 0, questId));
	outcome.packets = collectUntilQuiet(quiet, limit);
	outcome.closed = client.socket.isClosed();
	return outcome;
}

} // namespace aion::gameserver::scenario
