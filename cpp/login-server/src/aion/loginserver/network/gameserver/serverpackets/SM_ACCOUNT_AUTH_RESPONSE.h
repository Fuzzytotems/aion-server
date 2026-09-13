#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "aion/loginserver/model/AccountTime.h"
#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * In this packet LoginServer is answering on GameServer request about valid authentication data and also sends account name of user that is
 * authenticating on GameServer.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_ACCOUNT_AUTH_RESPONSE
 *
 * @author -Nemesiss-
 */
class SM_ACCOUNT_AUTH_RESPONSE : public GsServerPacket {
public:
	/**
	 * @param creationDate time of account creation, measured in milliseconds since 1.1.1970 UTC
	 * @param allowedHddSerial std::nullopt writes an empty string (Java: null)
	 * @param accountTime the account time whose accumulated online and rest time are sent if ok.
	 *          Deviation: Java looks the account up on the connection's GameServerInfo while writing the packet, which throws a
	 *          NullPointerException if the account left the game server meanwhile (e.g. a CM_ACCOUNT_DISCONNECTED processed before the IO thread
	 *          wrote this packet); the account time is taken when the response is created instead.
	 */
	SM_ACCOUNT_AUTH_RESPONSE(int32_t accountId, bool ok, std::string accountName, int64_t creationDate, int8_t accessLevel, int8_t membership,
		std::optional<std::string> allowedHddSerial, std::optional<model::AccountTime> accountTime)
		: accountId(accountId),
			ok(ok),
			accountName(std::move(accountName)),
			creationDate(creationDate),
			accessLevel(accessLevel),
			membership(membership),
			allowedHddSerial(std::move(allowedHddSerial)),
			accountTime(std::move(accountTime)) {}

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeC(buf, 1);
		writeD(buf, accountId);
		writeC(buf, ok ? 1 : 0);

		if (ok) {
			writeS(buf, accountName);
			writeQ(buf, creationDate);
			const model::AccountTime& time = accountTime.value();
			writeQ(buf, time.getAccumulatedOnlineTime());
			writeQ(buf, time.getAccumulatedRestTime());
			writeC(buf, accessLevel);
			writeC(buf, membership);
			writeS(buf, allowedHddSerial.value_or(""));
		}
	}

private:
	const int32_t accountId;
	/** True if account is authenticated. */
	const bool ok;
	const std::string accountName;
	/** Time of account creation, measured in milliseconds since 1.1.1970 UTC */
	const int64_t creationDate;
	const int8_t accessLevel, membership;
	const std::optional<std::string> allowedHddSerial;
	const std::optional<model::AccountTime> accountTime;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
