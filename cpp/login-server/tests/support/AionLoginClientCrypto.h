#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <openssl/bn.h>

#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/network/ncrypt/BlowfishCipher.h"
#include "aion/loginserver/network/ncrypt/OpenSslUtils.h"

namespace aion::loginserver::test {

/**
 * The CLIENT side of the Aion 4.8 login protocol encryption, for tests that talk to the login server like the game client does.
 * <p>
 * <b>Framing.</b> Every packet on the wire (both directions) is a frame <tt>[uint16 size, little endian][body]</tt> where size is the size of
 * the whole frame including the two size bytes. Only the body is encrypted. Frames passed to and returned by this class always include the
 * size header. Splitting a TCP stream into frames is up to the caller (read 2 bytes, then size - 2 more).
 * <p>
 * <b>Server to client.</b>
 * <ul>
 * <li>The first packet (SM_INIT) is Blowfish encrypted with the static INITIAL_KEY and obfuscated with a rolling XOR (decXORPass undoes it).
 * It carries the session id, the scrambled RSA modulus and the Blowfish session key. decryptInitPacket() parses it and switches this object
 * to the session key.</li>
 * <li>Every later body is Blowfish encrypted with the session key; its last 4 bytes are a checksum (XOR of all words is 0). decryptServerPacket()
 * returns the whole decrypted body: opcode, data, padding and checksum. Padding bytes may hold stale buffer contents. Note: the server
 * encrypts payload size - 2 bytes plus checksum/padding, so for payloads with size % 8 == 5 the checksum overwrites the last payload byte
 * (the first packet: the XOR key overwrites it for size % 8 == 1). This is the Java server's behaviour.</li>
 * </ul>
 * <b>Client to server.</b> encryptClientPacket() takes the payload (opcode followed by the packet data), zero-pads it (see padClientPacket:
 * at least 8 bytes after the payload rounded up to a multiple of 4, total a multiple of 8), puts a checksum (XOR of all preceding words) into
 * the last-but-one word and encrypts it with the session key. The server's check requires the XOR of all words except the last one to be 0,
 * so the checksum must not be the last word. For CM_LOGIN this produces exactly the 12 trailing bytes (0, checksum, 0) that the server's
 * CM_LOGIN.readImpl skips.
 * <p>
 * <b>Login data.</b> CM_LOGIN carries the credentials RSA encrypted (raw RSA, e = 65537, no padding) in 128-byte blocks, using the modulus
 * recovered from SM_INIT (decryptModulus, the inverse of EncryptedRSAKeyPair::encryptModulus). Layout of the plaintext (see buildLoginData):
 * <ul>
 * <li>normal client: one block, bytes 94-107 username (14 bytes), 108-123 password (16 bytes), 124-127 OTP (int32 little endian, -1 = none)</li>
 * <li>client started with -loginex: two blocks, each with content at bytes 78-127; the concatenated 100 content bytes are username (64 bytes),
 * password (32 bytes), OTP (4 bytes)</li>
 * </ul>
 * Strings are zero-padded or truncated to their field size, bytes taken as given (the server decodes Cp1252, so use ASCII).
 * <p>
 * Not thread-safe. Throws commons::utils::IllegalArgumentException / IllegalStateException on malformed input or misuse.
 */
class AionLoginClientCrypto {
public:
	/** static Blowfish key of the first server packet (independent copy of the server's constant) */
	static constexpr std::array<uint8_t, 16> INITIAL_KEY = {0x6b, 0x60, 0xcb, 0x5b, 0x82, 0xce, 0x90, 0xb1, 0xcc, 0x2b, 0x6c, 0x55, 0x6c, 0x6c, 0x6c, 0x6c};
	static constexpr size_t RSA_BLOCK_SIZE = 128;
	static constexpr unsigned long RSA_PUBLIC_EXPONENT = 65537;
	static constexpr size_t BLOWFISH_KEY_SIZE = 16;
	/** opcodes of the packets built here */
	static constexpr uint8_t SM_INIT_OPCODE = 0x00;
	static constexpr uint8_t CM_LOGIN_OPCODE = 0x00;
	/** SM_INIT body offsets (after decryption): opcode, session id, protocol revision, scrambled modulus, 16 zero bytes, Blowfish key */
	static constexpr size_t SM_INIT_SESSION_ID_OFFSET = 1;
	static constexpr size_t SM_INIT_REVISION_OFFSET = 5;
	static constexpr size_t SM_INIT_MODULUS_OFFSET = 9;
	static constexpr size_t SM_INIT_BLOWFISH_KEY_OFFSET = 9 + 128 + 16;

	/** Contents of a decrypted SM_INIT */
	struct InitPacket {
		/** the decrypted body (the XOR key word at size - 8 is left as it is) */
		std::vector<uint8_t> body;
		int32_t sessionId = 0;
		int32_t protocolRevision = 0;
		std::array<uint8_t, RSA_BLOCK_SIZE> encryptedModulus{};
		std::array<uint8_t, BLOWFISH_KEY_SIZE> blowfishKey{};
	};

	AionLoginClientCrypto() : cipher(INITIAL_KEY) {}

	/**
	 * Decrypts the first server packet (SM_INIT), parses it and switches to its Blowfish session key.
	 *
	 * @param frame
	 *          the complete frame including the size header
	 * @throws commons::utils::IllegalArgumentException if the frame is malformed or the opcode is not 0x00
	 * @throws commons::utils::IllegalStateException if a packet was already decrypted
	 */
	InitPacket decryptInitPacket(std::span<const uint8_t> frame) {
		using commons::utils::IllegalArgumentException;
		if (initialized)
			throw commons::utils::IllegalStateException("SM_INIT was already received");
		std::vector<uint8_t> body = frameBody(frame);
		if (body.size() < 16 || body.size() < SM_INIT_BLOWFISH_KEY_OFFSET + BLOWFISH_KEY_SIZE)
			throw IllegalArgumentException("SM_INIT body too short: " + std::to_string(body.size()));
		cipher.decipher(body);
		decXORPass(body);
		if (body[0] != SM_INIT_OPCODE)
			throw IllegalArgumentException("Not an SM_INIT packet, opcode " + std::to_string(body[0]));

		InitPacket packet;
		packet.sessionId = static_cast<int32_t>(readWord(body, SM_INIT_SESSION_ID_OFFSET));
		packet.protocolRevision = static_cast<int32_t>(readWord(body, SM_INIT_REVISION_OFFSET));
		std::copy_n(body.begin() + SM_INIT_MODULUS_OFFSET, RSA_BLOCK_SIZE, packet.encryptedModulus.begin());
		std::copy_n(body.begin() + SM_INIT_BLOWFISH_KEY_OFFSET, BLOWFISH_KEY_SIZE, packet.blowfishKey.begin());
		packet.body = std::move(body);

		sessionId = packet.sessionId;
		blowfishKey = packet.blowfishKey;
		modulus = decryptModulus(packet.encryptedModulus);
		cipher.updateKey(blowfishKey);
		initialized = true;
		return packet;
	}

	/**
	 * Decrypts a server packet received after SM_INIT and verifies its checksum.
	 *
	 * @return the decrypted body (opcode, data, padding, 4-byte checksum)
	 * @throws commons::utils::IllegalArgumentException if the frame is malformed or the checksum is wrong
	 * @throws commons::utils::IllegalStateException before decryptInitPacket()
	 */
	std::vector<uint8_t> decryptServerPacket(std::span<const uint8_t> frame) {
		requireInitialized();
		std::vector<uint8_t> body = frameBody(frame);
		if (body.empty())
			throw commons::utils::IllegalArgumentException("Empty server packet");
		cipher.decipher(body);
		if (!verifyServerChecksum(body))
			throw commons::utils::IllegalArgumentException("Wrong checksum in server packet");
		return body;
	}

	/**
	 * Pads, checksums and encrypts a client packet with the session key.
	 *
	 * @param payload
	 *          opcode followed by the packet data
	 * @return the complete frame including the size header
	 * @throws commons::utils::IllegalStateException before decryptInitPacket()
	 */
	std::vector<uint8_t> encryptClientPacket(std::span<const uint8_t> payload) const {
		requireInitialized();
		std::vector<uint8_t> body = padClientPacket(payload);
		cipher.cipher(body);
		return makeFrame(body);
	}

	/**
	 * RSA encrypts the login data with the modulus from SM_INIT.
	 *
	 * @return 128 bytes (normal) or 256 bytes (loginEx)
	 */
	std::vector<uint8_t> encryptLoginData(std::string_view username, std::string_view password, int32_t otp = -1, bool loginEx = false) const {
		requireInitialized();
		return rsaEncrypt(modulus, buildLoginData(username, password, otp, loginEx));
	}

	/**
	 * Builds the CM_LOGIN payload (not yet Blowfish encrypted, pass it to encryptClientPacket): opcode 0x00, RSA encrypted login data, session
	 * id, 16 zero bytes, 7 static bytes (20 00 00 00 00 00 01), 16 static bytes (9D DA 47 A7 21 C0 A6 A5 4B B7 5E E3 CE C9 26 AA).
	 *
	 * @param sessionIdOverride
	 *          session id to send instead of the one received in SM_INIT
	 */
	std::vector<uint8_t> buildCM_LOGIN(std::string_view username, std::string_view password, bool loginEx = false, int32_t otp = -1,
		std::optional<int32_t> sessionIdOverride = std::nullopt) const {
		std::vector<uint8_t> payload{CM_LOGIN_OPCODE};
		std::vector<uint8_t> loginData = encryptLoginData(username, password, otp, loginEx);
		payload.insert(payload.end(), loginData.begin(), loginData.end());
		appendWord(payload, static_cast<uint32_t>(sessionIdOverride.value_or(sessionId)));
		payload.insert(payload.end(), 16, uint8_t{0});
		static constexpr std::array<uint8_t, 7> STATIC_1 = {0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
		static constexpr std::array<uint8_t, 16> STATIC_2 = {0x9D, 0xDA, 0x47, 0xA7, 0x21, 0xC0, 0xA6, 0xA5, 0x4B, 0xB7, 0x5E, 0xE3, 0xCE, 0xC9, 0x26, 0xAA};
		payload.insert(payload.end(), STATIC_1.begin(), STATIC_1.end());
		payload.insert(payload.end(), STATIC_2.begin(), STATIC_2.end());
		return payload;
	}

	bool isInitialized() const noexcept { return initialized; }
	/** @return the session id from SM_INIT */
	int32_t getSessionId() const noexcept { return sessionId; }
	/** @return the Blowfish session key from SM_INIT */
	const std::array<uint8_t, BLOWFISH_KEY_SIZE>& getBlowfishKey() const noexcept { return blowfishKey; }
	/** @return the unscrambled RSA modulus from SM_INIT (unsigned big endian) */
	const std::array<uint8_t, RSA_BLOCK_SIZE>& getModulus() const noexcept { return modulus; }

	// ---- stateless building blocks ----

	/** @return frame of the given body: [uint16 LE (body size + 2)][body] */
	static std::vector<uint8_t> makeFrame(std::span<const uint8_t> body) {
		if (body.size() + 2 > 0xFFFF)
			throw commons::utils::IllegalArgumentException("Packet too large: " + std::to_string(body.size()));
		std::vector<uint8_t> frame(body.size() + 2);
		frame[0] = static_cast<uint8_t>(frame.size());
		frame[1] = static_cast<uint8_t>(frame.size() >> 8);
		std::ranges::copy(body, frame.begin() + 2);
		return frame;
	}

	/** @return a copy of the body of a frame, after checking that its size header matches the frame size */
	static std::vector<uint8_t> frameBody(std::span<const uint8_t> frame) {
		if (frame.size() < 2)
			throw commons::utils::IllegalArgumentException("Frame too short: " + std::to_string(frame.size()));
		size_t size = static_cast<size_t>(frame[0]) | static_cast<size_t>(frame[1]) << 8;
		if (size != frame.size())
			throw commons::utils::IllegalArgumentException("Frame size header " + std::to_string(size) + " does not match frame size " + std::to_string(frame.size()));
		return {frame.begin() + 2, frame.end()};
	}

	/**
	 * Undoes the server's encXORPass in place: the word at size - 8 (at 4 for an 8-byte packet) holds the final rolling key; walking backwards
	 * from the word before it down to offset 4, each word is XORed with the rolling key, which is then decreased by the result. data.size() must
	 * be a positive multiple of 8.
	 */
	static void decXORPass(std::span<uint8_t> data) {
		if (data.size() % 8 != 0 || data.empty())
			throw commons::utils::IllegalArgumentException("Invalid first packet size: " + std::to_string(data.size()));
		size_t keyPos = std::max<size_t>(data.size(), 12) - 8;
		uint32_t ecx = readWord(data, keyPos);
		for (size_t pos = keyPos - 4; pos >= 4; pos -= 4) {
			uint32_t edx = readWord(data, pos) ^ ecx;
			ecx -= edx;
			writeWord(data, pos, edx);
		}
	}

	/** @return true if the size is a positive multiple of 8 and the XOR of all 32-bit words (including the checksum word) is 0 */
	static bool verifyServerChecksum(std::span<const uint8_t> body) noexcept {
		if (body.empty() || body.size() % 8 != 0)
			return false;
		uint32_t chksum = 0;
		for (size_t i = 0; i < body.size(); i += 4)
			chksum ^= readWord(body, i);
		return chksum == 0;
	}

	/**
	 * @return the payload, zero-padded to size = the smallest multiple of 8 that is &gt;= (payload size rounded up to a multiple of 4) + 8, with
	 * the checksum (XOR of all preceding words) in the last-but-one word. The last word stays zero.
	 */
	static std::vector<uint8_t> padClientPacket(std::span<const uint8_t> payload) {
		size_t checksumPos = (payload.size() + 3) / 4 * 4;
		size_t size = (checksumPos + 8 + 7) / 8 * 8;
		checksumPos = size - 8;
		std::vector<uint8_t> body(size);
		std::ranges::copy(payload, body.begin());
		uint32_t chksum = 0;
		for (size_t i = 0; i < checksumPos; i += 4)
			chksum ^= readWord(body, i);
		writeWord(body, checksumPos, chksum);
		return body;
	}

	/** Inverse of EncryptedRSAKeyPair::encryptModulus for a 128-byte scrambled modulus. @return the modulus (unsigned big endian) */
	static std::array<uint8_t, RSA_BLOCK_SIZE> decryptModulus(std::span<const uint8_t> encryptedModulus) {
		if (encryptedModulus.size() != RSA_BLOCK_SIZE)
			throw commons::utils::IllegalArgumentException("Scrambled modulus must have 128 bytes, not " + std::to_string(encryptedModulus.size()));
		std::array<uint8_t, RSA_BLOCK_SIZE> m{};
		std::ranges::copy(encryptedModulus, m.begin());
		// the server's steps in reverse order (each step is its own inverse)
		for (size_t i = 0; i < 0x40; i++)
			m[0x40 + i] ^= m[i];
		for (size_t i = 0; i < 4; i++)
			m[0x0d + i] ^= m[0x34 + i];
		for (size_t i = 0; i < 0x40; i++)
			m[i] ^= m[0x40 + i];
		for (size_t i = 0; i < 4; i++)
			std::swap(m[i], m[0x4d + i]);
		return m;
	}

	/**
	 * @return the plaintext login data as CM_LOGIN.decryptLoginData expects it after RSA decryption: 128 bytes (normal) or 256 bytes (loginEx)
	 */
	static std::vector<uint8_t> buildLoginData(std::string_view username, std::string_view password, int32_t otp = -1, bool loginEx = false) {
		size_t blocks = loginEx ? 2 : 1;
		size_t contentStart = loginEx ? 78 : 94;
		size_t usernameLength = loginEx ? 64 : 14;
		size_t passwordLength = loginEx ? 32 : 16;

		std::vector<uint8_t> content(usernameLength + passwordLength + 4);
		std::copy_n(username.begin(), std::min(username.size(), usernameLength), content.begin());
		std::copy_n(password.begin(), std::min(password.size(), passwordLength), content.begin() + static_cast<ptrdiff_t>(usernameLength));
		writeWord(content, usernameLength + passwordLength, static_cast<uint32_t>(otp));

		size_t contentPerBlock = RSA_BLOCK_SIZE - contentStart;
		std::vector<uint8_t> data(blocks * RSA_BLOCK_SIZE);
		for (size_t block = 0; block < blocks; block++) {
			std::copy_n(content.begin() + static_cast<ptrdiff_t>(block * contentPerBlock), contentPerBlock,
				data.begin() + static_cast<ptrdiff_t>(block * RSA_BLOCK_SIZE + contentStart));
		}
		return data;
	}

	/**
	 * Raw RSA encryption (c = m^65537 mod n) of each 128-byte block, without padding.
	 *
	 * @param modulus
	 *          unsigned big endian modulus
	 * @param plain
	 *          a multiple of 128 bytes; each block (big endian) must be smaller than the modulus
	 * @return the encrypted blocks, each left-padded to 128 bytes
	 */
	static std::vector<uint8_t> rsaEncrypt(std::span<const uint8_t> modulus, std::span<const uint8_t> plain) {
		using namespace network::ncrypt;
		if (plain.size() % RSA_BLOCK_SIZE != 0)
			throw commons::utils::IllegalArgumentException("RSA plaintext size is not a multiple of 128: " + std::to_string(plain.size()));
		BnCtxPtr ctx(BN_CTX_new());
		BignumPtr n(BN_bin2bn(modulus.data(), static_cast<int>(modulus.size()), nullptr));
		BignumPtr e(BN_new());
		BignumPtr m(BN_new());
		BignumPtr c(BN_new());
		if (!ctx || !n || !e || !m || !c || !BN_set_word(e.get(), RSA_PUBLIC_EXPONENT))
			throwOpenSslException("Could not allocate RSA numbers");
		if (BN_num_bytes(n.get()) > static_cast<int>(RSA_BLOCK_SIZE))
			throw commons::utils::IllegalArgumentException("Modulus is longer than 128 bytes");

		std::vector<uint8_t> encrypted(plain.size());
		for (size_t offset = 0; offset < plain.size(); offset += RSA_BLOCK_SIZE) {
			if (!BN_bin2bn(plain.data() + offset, static_cast<int>(RSA_BLOCK_SIZE), m.get()))
				throwOpenSslException("BN_bin2bn failed");
			if (BN_cmp(m.get(), n.get()) >= 0)
				throw commons::utils::IllegalArgumentException("RSA block is not smaller than the modulus");
			if (!BN_mod_exp(c.get(), m.get(), e.get(), n.get(), ctx.get()))
				throwOpenSslException("BN_mod_exp failed");
			if (BN_bn2binpad(c.get(), encrypted.data() + offset, static_cast<int>(RSA_BLOCK_SIZE)) != static_cast<int>(RSA_BLOCK_SIZE))
				throwOpenSslException("BN_bn2binpad failed");
		}
		return encrypted;
	}

	static uint32_t readWord(std::span<const uint8_t> data, size_t pos) noexcept {
		return static_cast<uint32_t>(data[pos]) | static_cast<uint32_t>(data[pos + 1]) << 8 | static_cast<uint32_t>(data[pos + 2]) << 16 |
			static_cast<uint32_t>(data[pos + 3]) << 24;
	}

	static void writeWord(std::span<uint8_t> data, size_t pos, uint32_t value) noexcept {
		data[pos] = static_cast<uint8_t>(value);
		data[pos + 1] = static_cast<uint8_t>(value >> 8);
		data[pos + 2] = static_cast<uint8_t>(value >> 16);
		data[pos + 3] = static_cast<uint8_t>(value >> 24);
	}

	static void appendWord(std::vector<uint8_t>& data, uint32_t value) {
		data.resize(data.size() + 4);
		writeWord(data, data.size() - 4, value);
	}

private:
	void requireInitialized() const {
		if (!initialized)
			throw commons::utils::IllegalStateException("SM_INIT was not received yet");
	}

	network::ncrypt::BlowfishCipher cipher;
	bool initialized = false;
	int32_t sessionId = 0;
	std::array<uint8_t, BLOWFISH_KEY_SIZE> blowfishKey{};
	std::array<uint8_t, RSA_BLOCK_SIZE> modulus{};
};

} // namespace aion::loginserver::test
