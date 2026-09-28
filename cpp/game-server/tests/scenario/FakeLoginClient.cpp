#include "FakeLoginClient.h"

#include <stdexcept>

namespace aion::gameserver::scenario {

using network::test::PacketReader;
using network::test::PacketWriter;

FakeLoginClient::FakeLoginClient(uint16_t port) : socket(port) {
}

std::optional<std::vector<uint8_t>> FakeLoginClient::readPacket(std::chrono::milliseconds timeout) {
	std::optional<std::vector<uint8_t>> frame = socket.readFrame(timeout);
	if (!frame)
		return std::nullopt;
	return crypto.decryptServerPacket(*frame);
}

void FakeLoginClient::sendPacket(std::span<const uint8_t> payload) {
	socket.send(crypto.encryptClientPacket(payload));
}

std::vector<uint8_t> FakeLoginClient::expectPacket(uint8_t opcode, std::string_view step) {
	std::optional<std::vector<uint8_t>> packet = readPacket();
	if (!packet)
		throw std::runtime_error(std::string(step) + ": no answer from the login server (closed: " + (socket.isClosed() ? "yes" : "no") + ")");
	if (packet->empty() || (*packet)[0] != opcode) {
		std::string description = packet->empty() ? "an empty packet" : "opcode " + std::to_string((*packet)[0]);
		if (!packet->empty() && ((*packet)[0] == SM_LOGIN_FAIL || (*packet)[0] == SM_PLAY_FAIL) && packet->size() >= 5)
			description += " with reason " + std::to_string(PacketReader(std::span<const uint8_t>(*packet).subspan(1)).D());
		throw std::runtime_error(std::string(step) + ": expected opcode " + std::to_string(opcode) + ", got " + description);
	}
	return *packet;
}

void FakeLoginClient::login(std::string_view account, std::string_view password) {
	std::optional<std::vector<uint8_t>> init = socket.readFrame(std::chrono::seconds(10));
	if (!init)
		throw std::runtime_error("SM_INIT: no packet from the login server");
	crypto.decryptInitPacket(*init);
	sendPacket(buildCM_AUTH_GG(crypto.getSessionId()));
	expectPacket(SM_AUTH_GG, "CM_AUTH_GG");
	sendPacket(crypto.buildCM_LOGIN(account, password));
	std::vector<uint8_t> ok = expectPacket(SM_LOGIN_OK, "CM_LOGIN");
	PacketReader reader(std::span<const uint8_t>(ok).subspan(1));
	key.accountId = reader.D();
	key.loginOk = reader.D();
}

FakeLoginClient::ServerList FakeLoginClient::requestServerList() {
	sendPacket(buildCM_SERVER_LIST(key.accountId, key.loginOk));
	return parseServerList(expectPacket(SM_SERVER_LIST, "CM_SERVER_LIST"));
}

FakeLoginClient::SessionKey FakeLoginClient::play(int8_t serverId) {
	sendPacket(buildCM_PLAY(key.accountId, key.loginOk, serverId));
	std::vector<uint8_t> ok = expectPacket(SM_PLAY_OK, "CM_PLAY");
	PacketReader reader(std::span<const uint8_t>(ok).subspan(1));
	key.playOk1 = reader.D();
	key.playOk2 = reader.D();
	return key;
}

std::vector<uint8_t> FakeLoginClient::buildCM_AUTH_GG(int32_t sessionId) {
	// CM_AUTH_GG.readImpl: sessionId, 4 x readD, readB(0x0B); the trailing unknown bytes are the client's padding and checksum (the crypto)
	return PacketWriter().C(CM_AUTH_GG).D(sessionId).D(0).D(0).D(0).D(0).data;
}

std::vector<uint8_t> FakeLoginClient::buildCM_SERVER_LIST(int32_t accountId, int32_t loginOk) {
	// CM_SERVER_LIST.readImpl: accountId, loginOk, readC (always 7), readB(6), then two random words (padding and checksum)
	return PacketWriter().C(CM_SERVER_LIST).D(accountId).D(loginOk).C(7).zeros(6).data;
}

std::vector<uint8_t> FakeLoginClient::buildCM_PLAY(int32_t accountId, int32_t loginOk, int8_t serverId) {
	// CM_PLAY.readImpl: accountId, loginOk, servId (readC), readB(6), readQ (random: the padding and checksum words)
	return PacketWriter().C(CM_PLAY).D(accountId).D(loginOk).C(serverId).zeros(6).data;
}

FakeLoginClient::ServerList FakeLoginClient::parseServerList(std::span<const uint8_t> body) {
	PacketReader reader(body);
	if (reader.C() != SM_SERVER_LIST)
		throw std::runtime_error("not an SM_SERVER_LIST");
	ServerList list;
	const int32_t count = reader.C();
	list.lastServer = static_cast<int8_t>(reader.C());
	for (int32_t i = 0; i < count; i++) {
		GameServerEntry entry;
		entry.id = static_cast<int8_t>(reader.C());
		std::vector<uint8_t> ip = reader.B(4);
		entry.ip = std::to_string(ip[0]) + "." + std::to_string(ip[1]) + "." + std::to_string(ip[2]) + "." + std::to_string(ip[3]);
		entry.port = static_cast<uint16_t>(reader.H());
		reader.H(); // unk, always 0
		reader.C(); // age limit
		reader.C(); // pvp
		entry.currentPlayers = static_cast<uint16_t>(reader.H());
		entry.maxPlayers = static_cast<uint16_t>(reader.H());
		entry.online = reader.C() == 1;
		reader.C(); // server type
		reader.C(); // hidden
		reader.H(); // unk
		reader.C(); // brackets
		list.servers.push_back(entry);
	}
	const int32_t maxIdWithCharsPlusOne = static_cast<uint16_t>(reader.H());
	reader.C(); // "last server" button
	for (int32_t id = 1; id < maxIdWithCharsPlusOne; id++)
		list.characterCounts.push_back(reader.C());
	return list;
}

} // namespace aion::gameserver::scenario
