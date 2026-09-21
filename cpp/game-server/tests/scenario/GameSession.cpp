#include "GameSession.h"

#include <stdexcept>

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

} // namespace aion::gameserver::scenario
