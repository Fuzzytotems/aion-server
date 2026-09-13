#pragma once

#include <cstdint>
#include <optional>

#include "aion/commons/database/SqlTypes.h"

namespace aion::loginserver::model {

using commons::database::Timestamp;

/**
 * Class for storing account time data (last login time, last session duration time, accumulated online time today, accumulated rest time
 * today).
 * <p>
 * A value type: not synchronized. Account keeps its AccountTime under its own lock and hands out copies (see Account::getAccountTime and
 * Account::modifyAccountTime).
 * <p>
 * Java: com.aionemu.loginserver.model.AccountTime
 *
 * @author EvilSpirit
 */
class AccountTime {
public:
	/** Default constructor. Set the lastLoginTime to current time */
	AccountTime();

	/**
	 * @return time the account has last logged in.
	 * Deviation: never null (Java: nullable, but it is set by the constructor and account_time.last_active is NOT NULL).
	 */
	Timestamp getLastLoginTime() const noexcept { return lastLoginTime; }
	void setLastLoginTime(Timestamp value) noexcept { lastLoginTime = value; }

	/** @return the duration of the session in milliseconds */
	int64_t getSessionDuration() const noexcept { return sessionDuration; }
	void setSessionDuration(int64_t value) noexcept { sessionDuration = value; }

	/** @return accumulated online time in milliseconds */
	int64_t getAccumulatedOnlineTime() const noexcept { return accumulatedOnlineTime; }
	void setAccumulatedOnlineTime(int64_t value) noexcept { accumulatedOnlineTime = value; }

	/** @return accumulated rest time in milliseconds */
	int64_t getAccumulatedRestTime() const noexcept { return accumulatedRestTime; }
	void setAccumulatedRestTime(int64_t value) noexcept { accumulatedRestTime = value; }

	/** @return time after which the account expires, std::nullopt if it never expires */
	std::optional<Timestamp> getExpirationTime() const noexcept { return expirationTime; }
	void setExpirationTime(std::optional<Timestamp> value) noexcept { expirationTime = value; }

	/** @return time when the penalty (account ban) ends, std::nullopt if there is none (a value of 1000 ms means infinite, see CM_BAN) */
	std::optional<Timestamp> getPenaltyEnd() const noexcept { return penaltyEnd; }
	void setPenaltyEnd(std::optional<Timestamp> value) noexcept { penaltyEnd = value; }

	bool operator==(const AccountTime&) const = default;

private:
	/** Time the account has last logged in */
	Timestamp lastLoginTime;
	/** Time after the account will expired */
	std::optional<Timestamp> expirationTime;
	/** Time when the penalty will end */
	std::optional<Timestamp> penaltyEnd;
	/** The duration of the session */
	int64_t sessionDuration = 0;
	/** Accumulated Online Time */
	int64_t accumulatedOnlineTime = 0;
	/** Accumulated Rest Time */
	int64_t accumulatedRestTime = 0;
};

/** @return the current time with millisecond precision (Java: new Timestamp(System.currentTimeMillis())) */
Timestamp currentTimestamp() noexcept;

} // namespace aion::loginserver::model
