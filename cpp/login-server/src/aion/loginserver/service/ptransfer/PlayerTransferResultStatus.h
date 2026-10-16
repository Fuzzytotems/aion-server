#pragma once

#include <cstdint>

namespace aion::loginserver::service::ptransfer {

/**
 * The kinds of SM_PTRANSFER_RESPONSE; the enumerator values are the ids sent to the game server.
 * <p>
 * Java: com.aionemu.loginserver.service.ptransfer.PlayerTransferResultStatus
 *
 * @author KID
 */
enum class PlayerTransferResultStatus : int32_t { SEND_INFO = 20, OK = 21, ERROR_ = 22, PERFORM_ACTION = 23 };

/** Java: getId() */
constexpr int32_t getId(PlayerTransferResultStatus status) noexcept {
	return static_cast<int32_t>(status);
}

} // namespace aion::loginserver::service::ptransfer
