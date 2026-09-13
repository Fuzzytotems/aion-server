#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "aion/loginserver/network/aion/AionClientPacket.h"

namespace aion::loginserver::network::aion::clientpackets {

/**
 * The login request with the RSA encrypted credentials.
 * <p>
 * Byte array containing RSA encrypted credentials. The last four bytes might be the OTP (-1 if not used, likely requested via AC_OTPCHECK_REQ)<br>
 * <br>
 * Starting the client without <code>-loginex</code> parameter, the username and pw are sent together in one 128 byte chunk. In this case, the
 * username can be 14 characters long and the password 16<br>
 * The offset where user and pw start seem to be fixed. Unused characters are zero padded, excessive characters are truncated.<br>
 * Decrypted credentials example for user: abcdefghijklmn pw: abcdefghijklmnop
 * <pre>
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 61 62
 * 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70 FF FF FF FF
 * abcdefghijklmnabcdefghijklmnop????
 * </pre>
 * Using <code>-loginex</code>, the username can be up to 64 characters long and the password 32. Both are sent split over two chunks<br>
 * Decrypted chunk example for user: abcdefghijklmnopqrsabcdefghijklmnopqrsabcdefghijklmno3432432pqrs pw: 11111111111111111111111111111111
 * <pre>
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70 71 72
 * 73 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70 71 72 73 61 62 63 64 65 66 67 68 69 6A 6B 6C
 * abcdefghijklmnopqrsabcdefghijklmnopqrsabcdefghijkl
 * </pre>
 * <pre>
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 6D 6E 6F 33 34 33 32 34 33 32 70 71 72 73 31 31 31 31
 * 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 FF FF FF FF
 * mno3432432pqrs11111111111111111111111111111111????
 * </pre>
 * Successful and failed brute force bans are logged by this packet's logger, which LoginServer writes to log/cm_login.log only (logback.xml).
 * <p>
 * Java: com.aionemu.loginserver.network.aion.clientpackets.CM_LOGIN
 *
 * @author -Nemesiss-, KID, Lyahim, Rolandas, Neon
 */
class CM_LOGIN : public AionClientPacket {
public:
	/** Java: record LoginData(String username, String password, int otp) */
	struct LoginData {
		std::string username;
		std::string password;
		int32_t otp = 0;
	};

	CM_LOGIN(commons::utils::ByteBuffer buf, std::shared_ptr<LoginConnection> client, int32_t opCode)
		: AionClientPacket(std::move(buf), std::move(client), opCode) {}

	/**
	 * Parses RSA decrypted login data (the part of decryptLoginData after the decryption), exposed for tests.
	 *
	 * @param decrypted the decrypted blocks (128 bytes normally, a multiple of 128 with -loginex)
	 * @throws commons::utils::IndexOutOfBoundsException if the data is too short (Java: IndexOutOfBoundsException)
	 */
	static LoginData parseLoginData(std::vector<uint8_t> decrypted);

	/** Decodes windows-1252 bytes to UTF-8 (Java: new String(bytes, "Cp1252"), unmapped bytes become U+FFFD). Exposed for tests. */
	static std::string decodeCp1252(std::span<const uint8_t> bytes);

protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** @return the decrypted login data, std::nullopt if the RSA decryption failed */
	std::optional<LoginData> decryptLoginData() const;

	std::vector<uint8_t> encryptedLoginData;
	int32_t sessionId = 0;
};

} // namespace aion::loginserver::network::aion::clientpackets
