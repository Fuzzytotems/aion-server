#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace aion::loginserver::service::ptransfer {

/**
 * A row of the player_transfers table (see PlayerTransferDAO). A value type.
 * <p>
 * Java: com.aionemu.loginserver.service.ptransfer.PlayerTransferTask
 *
 * @author KID
 */
struct PlayerTransferTask {
	int32_t sourceAccountId = 0, targetAccountId = 0, playerId = 0;
	int8_t sourceServerId = 0, targetServerId = 0;
	int32_t id = 0;
	int8_t status = 0;
	/** std::nullopt: NULL (Java: null) */
	std::optional<std::string> comment;

	static constexpr int8_t STATUS_WAIT = 0, STATUS_ACTIVE = 1, STATUS_DONE = 2, STATUS_ERROR = 3;
};

} // namespace aion::loginserver::service::ptransfer
