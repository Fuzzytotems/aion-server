#include "aion/loginserver/network/aion/clientpackets/CM_LOGIN.h"

#include <algorithm>
#include <array>
#include <chrono>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/controller/BannedIpController.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/aion/serverpackets/SM_LOGIN_FAIL.h"
#include "aion/loginserver/network/aion/serverpackets/SM_LOGIN_OK.h"
#include "aion/loginserver/utils/BruteForceProtector.h"

namespace aion::loginserver::network::aion::clientpackets {

using configs::Config;
using namespace serverpackets;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.aion.clientpackets.CM_LOGIN"));
	return *logger;
}

/** windows-1252 code points of the bytes 0x80-0x9F (0xFFFD: unmapped) */
constexpr std::array<char16_t, 32> CP1252_80_9F = {
	0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
	0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178,
};

void appendUtf8(std::string& out, char16_t codePoint) {
	if (codePoint < 0x80) {
		out += static_cast<char>(codePoint);
	} else if (codePoint < 0x800) {
		out += static_cast<char>(0xC0 | (codePoint >> 6));
		out += static_cast<char>(0x80 | (codePoint & 0x3F));
	} else {
		out += static_cast<char>(0xE0 | (codePoint >> 12));
		out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (codePoint & 0x3F));
	}
}

std::string readString(std::span<const uint8_t> bytes, size_t offset, size_t maxLength) {
	size_t length = 0;
	for (size_t i = offset; length < maxLength && i < bytes.size() && bytes[i] != 0; i++)
		length++;
	if (offset + length > bytes.size())
		throw commons::utils::IndexOutOfBoundsException("offset " + std::to_string(offset) + ", length " + std::to_string(bytes.size()));
	return CM_LOGIN::decodeCp1252(bytes.subspan(offset, length));
}

} // namespace

void CM_LOGIN::readImpl() {
	encryptedLoginData = readB(getRemainingBytes() - 55);
	sessionId = readD();
	readB(16); // 0
	readB(7); // static bytes: 20 00 00 00 00 00 01
	readB(16); // static bytes: 9D DA 47 A7 21 C0 A6 A5 4B B7 5E E3 CE C9 26 AA
	readD(); // 0
	readD(); // unk
	readD(); // 0
}

void CM_LOGIN::runImpl() {
	if (getConnection()->getSessionId() != sessionId) {
		sendPacket(std::make_shared<SM_LOGIN_FAIL>(AionAuthResponse::STR_L2AUTH_S_SYSTEM_ERROR));
		return;
	}
	std::optional<LoginData> loginData = decryptLoginData();
	if (!loginData) {
		sendPacket(std::make_shared<SM_LOGIN_FAIL>(AionAuthResponse::STR_L2AUTH_S_SYSTEM_ERROR));
		return;
	}
	const std::shared_ptr<LoginConnection>& connection = getConnection();
	LoginConnection::AccountAttachScope accountAttachScope(*connection); // logs out if the client disconnects before the login finished
	std::optional<AionAuthResponse> response = controller::AccountController::login(loginData->username, loginData->password, connection);
	if (!response) // e.g. when account is banned
		return;
	switch (*response) {
		case AionAuthResponse::STR_L2AUTH_S_ALL_OK:
			connection->setState(LoginConnection::State::AUTHED_LOGIN);
			connection->setSessionKey(SessionKey(*connection->getAccount()));
			connection->sendPacket(std::make_shared<SM_LOGIN_OK>(connection->getSessionKey().value()));
			break;
		case AionAuthResponse::STR_L2AUTH_S_INVALID_ACCOUT:
		case AionAuthResponse::STR_L2AUTH_S_INCORRECT_PWD:
			if (Config::ENABLE_BRUTEFORCE_PROTECTION) {
				const std::string& ip = connection->getIP();
				if (ip != "127.0.0.1" && utils::BruteForceProtector::getInstance().addFailedConnect(ip)) {
					// Java: Config.WRONG_LOGIN_BAN_TIME * 60000 is an int multiplication, which wraps
					const int32_t banTimeMillis = static_cast<int32_t>(static_cast<uint32_t>(Config::WRONG_LOGIN_BAN_TIME) * 60000u);
					commons::database::Timestamp newTime{std::chrono::milliseconds(commons::utils::currentTimeMillis() + banTimeMillis)};
					controller::BannedIpController::banIp(ip, newTime);
					log().info(loginData->username + " on " + ip + " banned for " + std::to_string(Config::WRONG_LOGIN_BAN_TIME) + " min. bruteforce");
					connection->close(std::make_shared<SM_LOGIN_FAIL>(AionAuthResponse::STR_L2AUTH_S_BLOCKED_IP));
					break;
				}
			}
			[[fallthrough]];
		default:
			connection->sendPacket(std::make_shared<SM_LOGIN_FAIL>(*response));
			break;
	}
}

std::optional<CM_LOGIN::LoginData> CM_LOGIN::decryptLoginData() const {
	std::optional<std::vector<uint8_t>> decrypted = getConnection()->getEncryptedRSAKeyPair()->decrypt(encryptedLoginData);
	if (!decrypted)
		return std::nullopt;
	return parseLoginData(std::move(*decrypted));
}

CM_LOGIN::LoginData CM_LOGIN::parseLoginData(std::vector<uint8_t> decrypted) {
	const size_t length = decrypted.size();
	const bool isLoginEx = length > 128; // client was started with -loginex parameter
	const size_t contentStartOffset = isLoginEx ? 78 : 94; // decrypted chunks start with the same zero-padding
	const size_t usernameByteLength = isLoginEx ? 64 : 14;
	const size_t passwordByteLength = isLoginEx ? 32 : 16; // we can't detect -pwd16 (also limits to 16 characters), but handling zero-termination is sufficient
	if (length >= 128) {
		// shift to the left to remove prefix zero-padding
		for (size_t offset = length - 128;; offset -= 128) {
			std::copy(decrypted.begin() + static_cast<ptrdiff_t>(offset + contentStartOffset), decrypted.end(), decrypted.begin() + static_cast<ptrdiff_t>(offset));
			if (offset < 128)
				break;
		}
	}
	// Java: new String(bytes, offset, length, "Cp1252") and ByteBuffer.wrap(bytes, offset, 4) throw IndexOutOfBoundsException for short data
	if (length < usernameByteLength + passwordByteLength + 4)
		throw commons::utils::IndexOutOfBoundsException("Login data too short: " + std::to_string(length) + " bytes");
	LoginData loginData;
	loginData.username = readString(decrypted, 0, usernameByteLength);
	loginData.password = readString(decrypted, usernameByteLength, passwordByteLength);
	const size_t otpOffset = usernameByteLength + passwordByteLength;
	loginData.otp = static_cast<int32_t>(static_cast<uint32_t>(decrypted[otpOffset]) | static_cast<uint32_t>(decrypted[otpOffset + 1]) << 8 |
		static_cast<uint32_t>(decrypted[otpOffset + 2]) << 16 | static_cast<uint32_t>(decrypted[otpOffset + 3]) << 24);
	return loginData;
}

std::string CM_LOGIN::decodeCp1252(std::span<const uint8_t> bytes) {
	std::string text;
	text.reserve(bytes.size());
	for (uint8_t b : bytes) {
		if (b >= 0x80 && b <= 0x9F)
			appendUtf8(text, CP1252_80_9F[b - 0x80]);
		else
			appendUtf8(text, static_cast<char16_t>(b));
	}
	return text;
}

} // namespace aion::loginserver::network::aion::clientpackets
