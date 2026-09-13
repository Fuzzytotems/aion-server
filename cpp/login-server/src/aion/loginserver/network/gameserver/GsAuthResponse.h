#pragma once

#include <cstdint>

namespace aion::loginserver::network::gameserver {

/**
 * This class contains possible response that LoginServer may send to gameserver if authentication fail etc.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.GsAuthResponse
 *
 * @author -Nemesiss-
 */
enum class GsAuthResponse : int8_t {
	/** Everything is OK */
	AUTHED = 0,
	/** Password/IP etc does not match. */
	NOT_AUTHED = 1,
	/** Requested id is not free */
	ALREADY_REGISTERED = 2
};

/** Java: getResponseId() - message Id that may be sent to client. */
constexpr int8_t getResponseId(GsAuthResponse response) noexcept {
	return static_cast<int8_t>(response);
}

} // namespace aion::loginserver::network::gameserver
