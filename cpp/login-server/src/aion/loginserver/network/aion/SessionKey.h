#pragma once

#include <cstdint>

namespace aion::loginserver::model {
class Account;
}

namespace aion::loginserver::network::aion {

/**
 * The session key of a logged in client: the account id and three random keys. The client gets loginOk with SM_LOGIN_OK and playOk1/playOk2
 * with SM_PLAY_OK, and presents all of them to the game server, which asks the login server to verify them (CM_ACCOUNT_AUTH).
 * <p>
 * An immutable value type.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.SessionKey
 *
 * @author -Nemesiss-
 */
struct SessionKey {
	/** accountId - will be used for authentication on Game Server side. */
	int32_t accountId = 0;
	/** login ok key */
	int32_t loginOk = 0;
	/** play ok1 key */
	int32_t playOk1 = 0;
	/** play ok2 key */
	int32_t playOk2 = 0;

	/**
	 * Create new SesionKey for this Account, with random keys.
	 * @throws std::bad_optional_access if the account has no id (Java: NullPointerException)
	 */
	explicit SessionKey(const model::Account& acc);

	/** Create new SesionKey with given values. */
	constexpr SessionKey(int32_t accountId, int32_t loginOk, int32_t playOk1, int32_t playOk2) noexcept
		: accountId(accountId), loginOk(loginOk), playOk1(playOk1), playOk2(playOk2) {}

	/** @return true if accountId and loginOk match this SessionKey */
	constexpr bool checkLogin(int32_t otherAccountId, int32_t otherLoginOk) const noexcept { return accountId == otherAccountId && loginOk == otherLoginOk; }

	/** @return true if key match this SessionKey. */
	constexpr bool checkSessionKey(const SessionKey& key) const noexcept {
		return playOk1 == key.playOk1 && accountId == key.accountId && playOk2 == key.playOk2 && loginOk == key.loginOk;
	}
};

} // namespace aion::loginserver::network::aion
