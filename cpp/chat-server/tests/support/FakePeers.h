#pragma once

// A fake game server and a fake Aion client for the chat server tests, plus the byte vectors of the packets the chat server sends. Every
// builder and every expected packet is transcribed by hand from the Java sources (the game server's SM_CS_* packets for what a game server
// sends, the chat server's readImpl/writeImpl for the rest); nothing here uses the chat server's code.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "TestUtils.h"

namespace aion::chatserver::test {

/** A chat channel request identifier: "@&lt;U+0001&gt;&lt;type&gt;_&lt;meta&gt;&lt;U+0001&gt;&lt;gameServerId&gt;.&lt;raceId&gt;.AION.KOR" */
inline std::string channelIdentifier(std::string_view type, std::string_view meta, int32_t gameServerId, int32_t raceId) {
	return "@\x01" + std::string(type) + "_" + std::string(meta) + "\x01" + std::to_string(gameServerId) + "." + std::to_string(raceId) + ".AION.KOR";
}

/** Java SM_CHAT_INI: size 10, opcode 0x31, 0x40, D 2, H 0 */
inline const Bytes SM_CHAT_INI_BYTES{0x0A, 0x00, 0x31, 0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};

/** Java SM_PLAYER_AUTH_RESPONSE (to the client): size 12, opcode 0x02, 0x40, H 1, D 0, H 0x0822 */
inline const Bytes SM_PLAYER_AUTH_RESPONSE_BYTES{0x0C, 0x00, 0x02, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x08};

/** Java SM_CHANNEL_RESPONSE: opcode 0x11, 0x40, D channelRequestId, H 0, D channelId */
inline Bytes expectedChannelResponse(int32_t channelRequestId, int32_t channelId) {
	return PacketWriter().C(0x11).C(0x40).D(channelRequestId).H(0x00).D(channelId).frame();
}

/**
 * Java SM_CHANNEL_MESSAGE: opcode 0x1A, C 0, D 0, D 0, D channelId, D sender id, D 0, C 0, H identifier length / 2, identifier bytes, H text
 * size / 2, text bytes (UTF-16LE)
 */
inline Bytes expectedChannelMessage(int32_t channelId, int32_t senderId, std::string_view senderIdentifier, std::span<const uint8_t> content) {
	Bytes identifier = ascii16(senderIdentifier);
	return PacketWriter()
		.C(0x1A)
		.C(0x00)
		.D(0x00)
		.D(0x00)
		.D(channelId)
		.D(senderId)
		.D(0x00)
		.C(0x00)
		.H(static_cast<int32_t>(identifier.size() / 2))
		.B(identifier)
		.H(static_cast<int32_t>(content.size() / 2))
		.B(content)
		.frame();
}

/** expectedChannelMessage of an ASCII text */
inline Bytes expectedChannelMessage(int32_t channelId, int32_t senderId, std::string_view senderIdentifier, std::string_view text) {
	return expectedChannelMessage(channelId, senderId, senderIdentifier, ascii16(text));
}

/** expectedChannelMessage of any text */
inline Bytes expectedChannelMessage(int32_t channelId, int32_t senderId, std::string_view senderIdentifier, std::u16string_view text) {
	return expectedChannelMessage(channelId, senderId, senderIdentifier, utf16le(text));
}

/** The game server side of the chat server link (Java: the game server's ChatServerConnection with its SM_CS_* packets). */
class FakeGameServer {
public:
	explicit FakeGameServer(uint16_t port) : socket(port) {}

	/** Java SM_CS_AUTH: opcode 0x00, C gameServerId, S password */
	static Bytes buildAuth(int32_t gameServerId, std::string_view password) { return PacketWriter().C(0x00).C(gameServerId).S(password).frame(); }

	/** Java SM_CS_PLAYER_AUTH: opcode 0x01, D playerId, S accName, S nick, D raceId, C accessLevel */
	static Bytes buildPlayerAuth(int32_t playerId, std::string_view accName, std::string_view nick, int32_t raceId, int32_t accessLevel) {
		return PacketWriter().C(0x01).D(playerId).S(accName).S(nick).D(raceId).C(accessLevel).frame();
	}

	/** Java SM_CS_PLAYER_LOGOUT: opcode 0x02, D playerId */
	static Bytes buildPlayerLogout(int32_t playerId) { return PacketWriter().C(0x02).D(playerId).frame(); }

	/** Java SM_CS_PLAYER_GAG: opcode 0x03, D playerId, Q gagTime */
	static Bytes buildPlayerGag(int32_t playerId, int64_t gagTime) { return PacketWriter().C(0x03).D(playerId).Q(gagTime).frame(); }

	void send(std::span<const uint8_t> frame) { socket.send(frame); }

	/** Sends SM_CS_AUTH and returns the answer frame (SM_GS_AUTH_RESPONSE), failing the test if there is none */
	Bytes authenticate(int32_t gameServerId, std::string_view password) {
		send(buildAuth(gameServerId, password));
		return expectFrame("SM_GS_AUTH_RESPONSE");
	}

	/**
	 * Registers a player (SM_CS_PLAYER_AUTH) and returns the token of the answer (SM_PLAYER_AUTH_RESPONSE: opcode 0x01, D playerId, C token
	 * length, token), checking the frame layout.
	 */
	Bytes registerPlayer(int32_t playerId, std::string_view accName, std::string_view nick, int32_t raceId, int32_t accessLevel = 0) {
		send(buildPlayerAuth(playerId, accName, nick, raceId, accessLevel));
		return expectPlayerAuthResponse(playerId);
	}

	/** Reads SM_PLAYER_AUTH_RESPONSE (see registerPlayer) and returns its token. */
	Bytes expectPlayerAuthResponse(int32_t playerId) {
		Bytes frame = expectFrame("SM_PLAYER_AUTH_RESPONSE");
		PacketReader reader(frame);
		EXPECT_EQ(reader.H(), static_cast<int32_t>(frame.size()));
		EXPECT_EQ(reader.C(), 0x01);
		EXPECT_EQ(reader.D(), playerId);
		int32_t tokenLength = reader.C();
		EXPECT_EQ(tokenLength, 48);
		Bytes token = reader.B(static_cast<size_t>(tokenLength));
		EXPECT_EQ(reader.remaining(), 0u);
		return token;
	}

	Bytes expectFrame(std::string_view what, std::chrono::milliseconds timeout = DEFAULT_TIMEOUT) {
		auto frame = socket.readFrame(timeout);
		if (!frame) {
			ADD_FAILURE() << "expected " << what << " from the chat server but got none (closed: " << socket.isClosed() << ")";
			return Bytes(64);
		}
		return *frame;
	}

	TestSocket socket;
};

/** An Aion client's chat connection (the packets as the chat server's CM_* readImpl read them). */
class FakeChatClient {
public:
	explicit FakeChatClient(uint16_t port) : socket(port) {}

	/** Java CM_CHAT_INI.readImpl: opcode 0x30, C, H, D, D, D */
	static Bytes buildChatIni() { return PacketWriter().C(0x30).C(0x40).H(0).D(0).D(0).D(0).frame(); }

	/**
	 * Java CM_PLAYER_AUTH.readImpl: opcode 0x05, B(2) separator (one UTF-16LE char, "@" by default), C 0, D 1, H game name length, game name
	 * "AION", D 27, D 1, D 0, D playerId, D 0, D 0, D 0, H identifier length, identifier ("Name@..."), H account name length, account name, H token
	 * length (in bytes), token
	 */
	static Bytes buildPlayerAuth(int32_t playerId, std::string_view nameIdentifier, std::string_view accountName, std::span<const uint8_t> token,
		char16_t separator = u'@') {
		Bytes game = ascii16("AION");
		Bytes identifier = ascii16(nameIdentifier);
		Bytes account = ascii16(accountName);
		return PacketWriter()
			.C(0x05)
			.B(utf16le(std::u16string_view(&separator, 1)))
			.C(0)
			.D(1)
			.H(static_cast<int32_t>(game.size() / 2))
			.B(game)
			.D(27)
			.D(1)
			.D(0)
			.D(playerId)
			.D(0)
			.D(0)
			.D(0)
			.H(static_cast<int32_t>(identifier.size() / 2))
			.B(identifier)
			.H(static_cast<int32_t>(account.size() / 2))
			.B(account)
			.H(static_cast<int32_t>(token.size()))
			.B(token)
			.frame();
	}

	/** Java CM_CHANNEL_REQUEST.readImpl: opcode 0x10, C 0x40, H 0, D channelRequestId, B(16), H identifier length, identifier, D 0 */
	static Bytes buildChannelRequest(int32_t channelRequestId, std::string_view identifier) {
		return channelRequest(channelRequestId, ascii16(identifier));
	}

	/** buildChannelRequest with any text */
	static Bytes buildChannelRequest(int32_t channelRequestId, std::u16string_view identifier) {
		return channelRequest(channelRequestId, utf16le(identifier));
	}

	/** Java CM_CHANNEL_MESSAGE.readImpl: opcode 0x18, H, C, D, D, D, D, D channelId, C, H content length, content */
	static Bytes buildChannelMessage(int32_t channelId, std::string_view text) { return channelMessage(channelId, ascii16(text)); }

	/** buildChannelMessage with any text */
	static Bytes buildChannelMessage(int32_t channelId, std::u16string_view text) { return channelMessage(channelId, utf16le(text)); }

	/** Java CM_CHANNEL_LEAVE.readImpl: opcode 0x12, C 0, H 0, B(16), D channelId */
	static Bytes buildChannelLeave(int32_t channelId) { return PacketWriter().C(0x12).C(0).H(0).zeros(16).D(channelId).frame(); }

	/** Java CM_PING.readImpl: opcode 0xFF, C 0, H 0, B(16) */
	static Bytes buildPing() { return PacketWriter().C(0xFF).C(0).H(0).zeros(16).frame(); }

	/** Java CM_PLAYER_INFO.readImpl: opcode 0x2C, C 0, H 0, C classId, D 0, D level, B(135) */
	static Bytes buildPlayerInfo(int32_t classId, int32_t level) {
		return PacketWriter().C(0x2C).C(0).H(0).C(classId).D(0).D(level).zeros(135).frame();
	}

	/**
	 * Java CM_CHANNEL_CREATE.readImpl: opcode 0x0B, C 0x40, H 0, D channelRequestId, B(16), H identifier length, identifier, B(7), H password
	 * length, password, H -1
	 */
	static Bytes buildChannelCreate(int32_t channelRequestId, std::string_view identifier, std::string_view password) {
		Bytes name = ascii16(identifier);
		Bytes pass = ascii16(password);
		return PacketWriter()
			.C(0x0B)
			.C(0x40)
			.H(0)
			.D(channelRequestId)
			.zeros(16)
			.H(static_cast<int32_t>(name.size() / 2))
			.B(name)
			.zeros(7)
			.H(static_cast<int32_t>(pass.size() / 2))
			.B(pass)
			.H(-1)
			.frame();
	}

	/** Java CM_CHANNEL_JOIN.readImpl: opcode 0x0D, C 0x40, H 0, D channelRequestId, B(16), H identifier length, identifier, H password length, password */
	static Bytes buildChannelJoin(int32_t channelRequestId, std::string_view identifier, std::string_view password) {
		Bytes name = ascii16(identifier);
		Bytes pass = ascii16(password);
		return PacketWriter()
			.C(0x0D)
			.C(0x40)
			.H(0)
			.D(channelRequestId)
			.zeros(16)
			.H(static_cast<int32_t>(name.size() / 2))
			.B(name)
			.H(static_cast<int32_t>(pass.size() / 2))
			.B(pass)
			.frame();
	}

	void send(std::span<const uint8_t> frame) { socket.send(frame); }

	Bytes expectFrame(std::string_view what, std::chrono::milliseconds timeout = DEFAULT_TIMEOUT) {
		auto frame = socket.readFrame(timeout);
		if (!frame) {
			ADD_FAILURE() << "expected " << what << " from the chat server but got none (closed: " << socket.isClosed() << ")";
			return Bytes(64);
		}
		return *frame;
	}

	/** CM_CHAT_INI -&gt; SM_CHAT_INI, then CM_PLAYER_AUTH -&gt; SM_PLAYER_AUTH_RESPONSE */
	void login(int32_t playerId, std::string_view nameIdentifier, std::string_view accountName, std::span<const uint8_t> token) {
		send(buildChatIni());
		EXPECT_EQ(expectFrame("SM_CHAT_INI"), SM_CHAT_INI_BYTES);
		send(buildPlayerAuth(playerId, nameIdentifier, accountName, token));
		EXPECT_EQ(expectFrame("SM_PLAYER_AUTH_RESPONSE"), SM_PLAYER_AUTH_RESPONSE_BYTES);
	}

	/** CM_CHANNEL_REQUEST -&gt; SM_CHANNEL_RESPONSE. @return the channel id of the response (after checking the frame layout) */
	int32_t joinChannel(int32_t channelRequestId, std::string_view identifier) {
		send(buildChannelRequest(channelRequestId, identifier));
		return expectChannelResponse(channelRequestId);
	}

	/** joinChannel with any text */
	int32_t joinChannel(int32_t channelRequestId, std::u16string_view identifier) {
		send(buildChannelRequest(channelRequestId, identifier));
		return expectChannelResponse(channelRequestId);
	}

	/** Reads SM_CHANNEL_RESPONSE. @return its channel id (after checking the frame layout) */
	int32_t expectChannelResponse(int32_t channelRequestId) {
		Bytes frame = expectFrame("SM_CHANNEL_RESPONSE");
		PacketReader reader(frame);
		reader.B(10); // size, opcode, 0x40, request id, H 0
		int32_t channelId = reader.D();
		EXPECT_EQ(frame, expectedChannelResponse(channelRequestId, channelId)) << hex(frame);
		return channelId;
	}

	TestSocket socket;

private:
	static Bytes channelRequest(int32_t channelRequestId, std::span<const uint8_t> text) {
		return PacketWriter().C(0x10).C(0x40).H(0).D(channelRequestId).zeros(16).H(static_cast<int32_t>(text.size() / 2)).B(text).D(0).frame();
	}

	static Bytes channelMessage(int32_t channelId, std::span<const uint8_t> content) {
		return PacketWriter().C(0x18).H(0).C(0).D(0).D(0).D(0).D(0).D(channelId).C(0).H(static_cast<int32_t>(content.size() / 2)).B(content).frame();
	}
};

} // namespace aion::chatserver::test
