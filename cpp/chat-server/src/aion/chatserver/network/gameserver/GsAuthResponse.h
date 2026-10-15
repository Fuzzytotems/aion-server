#pragma once

#include <cstdint>
#include <string_view>

namespace aion::chatserver::network::gameserver {

/**
 * This class contains possible response that LoginServer may send to gameserver if authentication fail etc.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.GsAuthResponse
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

/** Java: getResponseId() */
constexpr int8_t getResponseId(GsAuthResponse response) noexcept {
	return static_cast<int8_t>(response);
}

/** Java: name() */
constexpr std::string_view name(GsAuthResponse response) noexcept {
	switch (response) {
		case GsAuthResponse::AUTHED:
			return "AUTHED";
		case GsAuthResponse::NOT_AUTHED:
			return "NOT_AUTHED";
		case GsAuthResponse::ALREADY_REGISTERED:
			return "ALREADY_REGISTERED";
	}
	return {};
}

} // namespace aion::chatserver::network::gameserver
