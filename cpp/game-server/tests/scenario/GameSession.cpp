#include "GameSession.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "decoders/SkillDecoders.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::scenario {

using network::test::PacketWriter;

void CharacterAppearance::writeTo(PacketWriter& writer) const {
	writer.D(voice).D(skinRGB).D(hairRGB).D(eyeRGB).D(lipRGB);
	writer.C(face).C(hair).C(deco).C(tattoo).C(faceContour).C(expression).C(unknown4).C(jawLine).C(forehead);
	writer.C(eyeHeight).C(eyeSpace).C(eyeWidth).C(eyeSize).C(eyeShape).C(eyeAngle);
	writer.C(browHeight).C(browAngle).C(browShape);
	writer.C(nose).C(noseBridge).C(noseWidth).C(noseTip);
	writer.C(cheek).C(lipHeight).C(mouthSize).C(lipSize).C(smile).C(lipShape).C(jawHeight).C(chinJut).C(earShape).C(headSize);
	writer.C(neck).C(neckLength);
	writer.C(shoulderSize);
	writer.C(torso).C(chest).C(waist).C(hips);
	writer.C(armThickness);
	writer.C(handSize).C(legThickness);
	writer.C(footSize).C(facialRate);
	writer.C(unknown0).C(armLength).C(legLength).C(shoulders).C(faceShape);
	writer.C(unknownA).C(unknownB).C(unknownC);
	writer.F(height);
}

GameSession::GameSession(uint16_t port) : client(port) {
}

std::string GameSession::nameOf(int32_t opcode) {
	if (const auto* entry = network::aion::ServerPacketsOpcodes::findByOpcode(opcode))
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

std::vector<uint8_t> GameSession::buildCM_VERSION_CHECK(uint16_t clientVersion) {
	// readImpl: readUH aionClientVersion, readUH npcScriptInterfaceVersion, readD windowsEncoding, readD windowsVersion, readD windowsSubVersion,
	// readC liteInfo
	return PacketWriter().H(clientVersion).H(0).D(1252).D(10).D(0).C(2).data;
}

std::vector<uint8_t> GameSession::buildCM_L2AUTH_LOGIN_CHECK(int32_t playOk2, int32_t playOk1, int32_t accountId, int32_t loginOk) {
	// readImpl: playOk2, playOk1, accountId, loginOk, unk1, unk2
	return PacketWriter().D(playOk2).D(playOk1).D(accountId).D(loginOk).D(0).D(0).data;
}

std::vector<uint8_t> GameSession::buildCM_MAC_ADDRESS(std::string_view macAddress, std::string_view hddSerial) {
	// readImpl: readC unk, readUH routeSteps, routeSteps x readD, readS macAddress, readS hddSerial, readD local IP
	return PacketWriter().C(0).H(1).D(0x0100007F).S(macAddress).S(hddSerial).D(0x0100007F).data;
}

std::vector<uint8_t> GameSession::buildCM_TIME_CHECK(int32_t nanoTime) {
	return PacketWriter().D(nanoTime).data;
}

std::vector<uint8_t> GameSession::buildCM_CHARACTER_LIST(int32_t playOk2) {
	return PacketWriter().D(playOk2).data;
}

std::vector<uint8_t> GameSession::buildCM_PING() {
	return PacketWriter().H(0).data; // readH unk
}

std::vector<uint8_t> GameSession::buildCM_GAMEGUARD(std::span<const uint8_t> data) {
	return PacketWriter().D(static_cast<int32_t>(data.size())).B(data).data; // readD size, readB(size)
}

std::vector<uint8_t> GameSession::buildCM_SECURITY_TOKEN() {
	return {};
}

std::vector<uint8_t> GameSession::buildCM_CHECK_NICKNAME(std::string_view nick) {
	return PacketWriter().S(nick).data;
}

std::vector<uint8_t> GameSession::buildCM_CREATE_CHARACTER(int32_t accountId, std::string_view accountName, const NewCharacter& character,
	uint8_t type) {
	PacketWriter writer;
	writer.D(accountId).S(accountName);
	// readBasicInfo: readS(25) name (the string, then (25 - length) * 2 padding bytes), gender, race, player class
	writer.S(character.name);
	const int32_t length = static_cast<int32_t>(commons::utils::StringUtils::toUtf16(character.name).size());
	if (length < 25)
		writer.zeros(static_cast<size_t>((25 - length) * 2));
	writer.D(character.female ? 1 : 0).D(character.asmodian ? 1 : 0).D(character.playerClassId);
	character.appearance.writeTo(writer);
	writer.C(type); // readUC type
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_MAY_LOGIN_INTO_GAME() {
	return {};
}

std::vector<uint8_t> GameSession::buildCM_ENTER_WORLD(int32_t objectId) {
	return PacketWriter().D(objectId).data;
}

std::vector<uint8_t> GameSession::buildCM_LEVEL_READY() {
	return {};
}

std::vector<uint8_t> GameSession::buildCM_MOVE(float x, float y, float z, int8_t heading, int8_t type, float x2, float y2, float z2) {
	// readImpl: x, y, z, heading, type; POSITION|MANUAL: ABSOLUTE -> x2, y2, z2, otherwise the vector (the scenario sends ABSOLUTE)
	return PacketWriter().F(x).F(y).F(z).C(heading).C(type).F(x2).F(y2).F(z2).data;
}

std::vector<uint8_t> GameSession::buildCM_MOVE(float x, float y, float z, int8_t heading, int8_t type) {
	return PacketWriter().F(x).F(y).F(z).C(heading).C(type).data;
}

std::vector<uint8_t> GameSession::buildCM_QUIT(bool stayConnected) {
	return PacketWriter().C(stayConnected ? 1 : 0).data;
}

std::vector<uint8_t> GameSession::buildCM_CUSTOM_SETTINGS(uint16_t display, uint16_t deny) {
	return PacketWriter().H(display).H(deny).data;
}

std::vector<uint8_t> GameSession::buildCM_SUBZONE_CHANGE(uint8_t unk) {
	return PacketWriter().C(unk).data;
}

std::vector<uint8_t> GameSession::buildCM_TARGET_SELECT(int32_t targetObjectId, bool selectTargetOfTarget) {
	return PacketWriter().D(targetObjectId).C(selectTargetOfTarget ? 1 : 0).data;
}

std::vector<uint8_t> GameSession::buildCM_ATTACK(int32_t targetObjectId, uint8_t attackNo, uint16_t time, uint8_t type) {
	return PacketWriter().D(targetObjectId).C(attackNo).H(time).C(type).data;
}

std::vector<uint8_t> GameSession::buildCM_REVIVE(uint8_t reviveId) {
	return PacketWriter().C(reviveId).data;
}

std::vector<uint8_t> GameSession::buildCM_CASTSPELL(const CastRequest& request) {
	// readImpl: readUH spellid, readUC level, readUC targetType, the arm of CM_CASTSPELL.java:43-67, readUH hitTime, readD unk
	PacketWriter writer;
	writer.H(request.spellId).C(request.level).C(request.targetType);
	switch (request.targetType) {
		case 0:
		case 3:
		case 4:
			writer.D(request.targetObjectId); // :47
			break;
		case 1:
			writer.F(request.x).F(request.y).F(request.z); // :50-52
			break;
		case 2:
			writer.F(request.x).F(request.y).F(request.z); // :55-57
			for (int i = 0; i < 8; i++)
				writer.F(0.0f); // :58-65, "unk1" .. "unk8"
			break;
		default:
			break; // no arm: the switch has no default
	}
	writer.H(request.hitTime).D(request.unk); // :69-70
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_CASTSPELL(uint16_t spellId, uint8_t level, uint8_t targetType, int32_t targetObjectId, uint16_t hitTime) {
	if (targetType == 1 || targetType == 2)
		throw std::invalid_argument("CM_CASTSPELL target type " + std::to_string(targetType) + " reads a point, not an object id");
	CastRequest request;
	request.spellId = spellId;
	request.level = level;
	request.targetType = targetType;
	request.targetObjectId = targetObjectId;
	request.hitTime = hitTime;
	return buildCM_CASTSPELL(request);
}

std::vector<uint8_t> GameSession::buildCM_REMOVE_ALTERED_STATE(uint16_t skillId, uint8_t unk1, uint8_t unk2) {
	return PacketWriter().H(skillId).C(unk1).C(unk2).data;
}

std::vector<uint8_t> GameSession::buildCM_START_LOOT(int32_t targetObjectId, uint8_t action) {
	return PacketWriter().D(targetObjectId).C(action).data; // CM_START_LOOT.java:36-37
}

std::vector<uint8_t> GameSession::buildCM_LOOT_ITEM(int32_t targetObjectId, uint8_t index) {
	return PacketWriter().D(targetObjectId).C(index).data; // CM_LOOT_ITEM.java:24-25
}

std::vector<uint8_t> GameSession::buildCM_USE_ITEM(int32_t uniqueItemId, int8_t type, int32_t extra) {
	PacketWriter writer;
	writer.D(uniqueItemId).C(type); // CM_USE_ITEM.java:39-40
	if (type == 2 || type == 5 || type == 6)
		writer.D(extra); // :42-50: targetItemId, syncId or indexReturn
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_MOVE_ITEM(int32_t itemObjId, uint8_t source, uint8_t destination, int16_t slot) {
	return PacketWriter().D(itemObjId).C(source).C(destination).H(slot).data; // CM_MOVE_ITEM.java:26-29
}

std::vector<uint8_t> GameSession::buildCM_SPLIT_ITEM(int32_t sourceItemObjId, int64_t itemAmount, uint8_t sourceStorageType, int32_t destinationItemObjId,
	uint8_t destinationStorageType, int16_t slotNum) {
	// CM_SPLIT_ITEM.java:28-33
	return PacketWriter().D(sourceItemObjId).Q(itemAmount).C(sourceStorageType).D(destinationItemObjId).C(destinationStorageType).H(slotNum).data;
}

std::vector<uint8_t> GameSession::buildCM_REPLACE_ITEM(uint8_t sourceStorageType, int32_t sourceItemObjId, uint8_t replaceStorageType,
	int32_t replaceItemObjId) {
	return PacketWriter().C(sourceStorageType).D(sourceItemObjId).C(replaceStorageType).D(replaceItemObjId).data; // CM_REPLACE_ITEM.java:26-29
}

std::vector<uint8_t> GameSession::buildCM_MANASTONE(const ManastoneRequest& request) {
	PacketWriter writer;
	writer.C(request.actionType).C(request.targetFusedSlot).D(request.targetItemUniqueId); // CM_MANASTONE.java:40-42
	switch (request.actionType) {
		case 1:
		case 2:
		case 4:
		case 8:
			writer.D(request.stoneUniqueId).D(request.supplementUniqueId); // :48-49
			break;
		case MANASTONE_REMOVE:
			writer.C(request.slotNum).C(0).H(0).D(request.npcObjId); // :52-55, the readC and readH are dropped
			break;
		default:
			break; // no arm: the switch has no default
	}
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_MANASTONE(uint8_t actionType, uint8_t targetFusedSlot, int32_t targetItemUniqueId, int32_t stoneUniqueId,
	int32_t supplementUniqueId) {
	if (actionType == MANASTONE_REMOVE)
		throw std::invalid_argument("CM_MANASTONE action 3 reads a slot and an npc, not two item ids");
	ManastoneRequest request;
	request.actionType = actionType;
	request.targetFusedSlot = targetFusedSlot;
	request.targetItemUniqueId = targetItemUniqueId;
	request.stoneUniqueId = stoneUniqueId;
	request.supplementUniqueId = supplementUniqueId;
	return buildCM_MANASTONE(request);
}

std::vector<uint8_t> GameSession::buildCM_EQUIP_ITEM(uint8_t action, int64_t slot, int32_t itemObjId) {
	return PacketWriter().C(action).Q(slot).D(itemObjId).data; // CM_EQUIP_ITEM.java:30-32
}

std::vector<uint8_t> GameSession::buildCM_DELETE_ITEM(int32_t itemObjectId) {
	return PacketWriter().D(itemObjectId).data; // CM_DELETE_ITEM.java:27
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

} // namespace aion::gameserver::scenario
