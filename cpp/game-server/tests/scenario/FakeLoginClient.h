#pragma once

// FakeLoginClient (m5a-plan.md F-04, §5.2 step 1): the game client's login server conversation over the client side crypto of
// login-server/tests/support/AionLoginClientCrypto.h: SM_INIT, CM_AUTH_GG -> SM_AUTH_GG, CM_LOGIN -> SM_LOGIN_OK (the login server creates the
// account with loginserver.accounts.autocreate), CM_SERVER_LIST -> SM_SERVER_LIST, CM_PLAY -> SM_PLAY_OK. Packet layouts written from the Java
// login server packets (network/aion/clientpackets and serverpackets), independent of the C++ login server.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "AionLoginClientCrypto.h"
#include "NetworkTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {

class FakeLoginClient {
public:
	/** Java opcodes of the login protocol */
	static constexpr uint8_t CM_AUTH_GG = 0x07;
	static constexpr uint8_t CM_LOGIN = 0x00;
	static constexpr uint8_t CM_SERVER_LIST = 0x05;
	static constexpr uint8_t CM_PLAY = 0x02;
	static constexpr uint8_t SM_LOGIN_FAIL = 0x01;
	static constexpr uint8_t SM_LOGIN_OK = 0x03;
	static constexpr uint8_t SM_SERVER_LIST = 0x04;
	static constexpr uint8_t SM_PLAY_FAIL = 0x06;
	static constexpr uint8_t SM_PLAY_OK = 0x07;
	static constexpr uint8_t SM_AUTH_GG = 0x0b;

	/** One game server of SM_SERVER_LIST */
	struct GameServerEntry {
		int8_t id = 0;
		std::string ip;
		int32_t port = 0;
		int32_t currentPlayers = 0;
		int32_t maxPlayers = 0;
		bool online = false;
	};

	struct ServerList {
		int8_t lastServer = 0;
		std::vector<GameServerEntry> servers;
		/** characters per game server id 1..maxIdWithChars */
		std::vector<int32_t> characterCounts;
	};

	/** The session key the game server checks (CM_L2AUTH_LOGIN_CHECK) */
	struct SessionKey {
		int32_t accountId = 0;
		int32_t loginOk = 0;
		int32_t playOk1 = 0;
		int32_t playOk2 = 0;
	};

	explicit FakeLoginClient(uint16_t port);

	/** SM_INIT, CM_AUTH_GG -> SM_AUTH_GG, CM_LOGIN -> SM_LOGIN_OK. @throws std::runtime_error with the failure (e.g. SM_LOGIN_FAIL reason) */
	void login(std::string_view account, std::string_view password);

	/** CM_SERVER_LIST -> SM_SERVER_LIST (after login) */
	ServerList requestServerList();

	/** CM_PLAY -> SM_PLAY_OK. @throws std::runtime_error on SM_PLAY_FAIL (with its reason) or another packet */
	SessionKey play(int8_t serverId);

	/** the next decrypted server packet body (opcode first), std::nullopt on timeout or close */
	std::optional<std::vector<uint8_t>> readPacket(std::chrono::milliseconds timeout = std::chrono::seconds(10));

	void sendPacket(std::span<const uint8_t> payload);

	/** the account id and loginOk of SM_LOGIN_OK */
	const SessionKey& sessionKey() const noexcept { return key; }

	// payload builders (opcode followed by the leading fields of the Java readImpl; the crypto adds padding and checksum)
	static std::vector<uint8_t> buildCM_AUTH_GG(int32_t sessionId);
	static std::vector<uint8_t> buildCM_SERVER_LIST(int32_t accountId, int32_t loginOk);
	static std::vector<uint8_t> buildCM_PLAY(int32_t accountId, int32_t loginOk, int8_t serverId);

	/** SM_SERVER_LIST body (opcode first) as SM_SERVER_LIST.writeImpl writes it */
	static ServerList parseServerList(std::span<const uint8_t> body);

private:
	std::vector<uint8_t> expectPacket(uint8_t opcode, std::string_view step);

	network::test::TestSocket socket;
	loginserver::test::AionLoginClientCrypto crypto;
	SessionKey key;
};

} // namespace aion::gameserver::scenario
