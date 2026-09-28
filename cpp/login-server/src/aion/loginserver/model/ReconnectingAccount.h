#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include "aion/loginserver/model/Account.h"

namespace aion::loginserver::model {

/**
 * This object is storing Account and corresponding to it reconnectionKey for client that will be reconnecting to LoginServer from GameServer
 * using fast reconnect feature.
 * <p>
 * Immutable; the referenced Account is shared (see Account for its thread safety).
 * <p>
 * Java: com.aionemu.loginserver.model.ReconnectingAccount
 *
 * @author -Nemesiss-
 */
class ReconnectingAccount {
public:
	ReconnectingAccount(std::shared_ptr<Account> account, int32_t reconnectionKey) noexcept
		: account(std::move(account)), reconnectionKey(reconnectionKey) {}

	/** @return Account object of account that will be reconnecting */
	const std::shared_ptr<Account>& getAccount() const noexcept { return account; }

	/** @return reconnection key for this account */
	int32_t getReconnectionKey() const noexcept { return reconnectionKey; }

private:
	/** Account object of account that will be reconnecting. */
	std::shared_ptr<Account> account;
	/** Reconnection Key that will be used for authenticating */
	int32_t reconnectionKey;
};

} // namespace aion::loginserver::model
