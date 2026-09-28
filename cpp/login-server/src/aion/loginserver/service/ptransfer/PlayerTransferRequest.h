#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/service/ptransfer/PlayerTransferStatus.h"

namespace aion::loginserver::service::ptransfer {

/**
 * An active player transfer: the character data sent by the source server, waiting for the target server's result. Its fields are set before it
 * is shared (PlayerTransferService, SM_PTRANSFER_RESPONSE) and not changed afterwards; the accounts are thread safe themselves.
 * <p>
 * Java: com.aionemu.loginserver.service.ptransfer.PlayerTransferRequest
 *
 * @author KID
 */
struct PlayerTransferRequest {
	explicit PlayerTransferRequest(PlayerTransferStatus status) noexcept : status(status) {}

	PlayerTransferStatus status;
	int8_t serverId = 0;
	int8_t targetServerId = 0;
	std::shared_ptr<model::Account> targetAccount;
	std::vector<uint8_t> db;
	std::string name;
	int32_t targetAccountId = 0;
	std::shared_ptr<model::Account> account;
	std::shared_ptr<model::Account> saccount;
	int32_t taskId = 0;
};

} // namespace aion::loginserver::service::ptransfer
