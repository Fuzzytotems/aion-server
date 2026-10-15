#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "aion/commons/database/SqlTypes.h"

namespace aion::loginserver::model {

/**
 * This class represents banned ip.
 * <p>
 * A value type (not synchronized). Equality and hash are based on the mask only, like in Java, so it can be kept in a
 * std::unordered_set&lt;BannedIP&gt; (Java: HashSet, see BannedIpDAO::getAllBans).
 * <p>
 * Java: com.aionemu.loginserver.model.BannedIP
 *
 * @author SoulKeeper
 */
class BannedIP {
public:
	/**
	 * Checks if ban is still active
	 *
	 * @return true if ban is still active (no expiration time, or it lies in the future)
	 */
	bool isActive() const noexcept;

	/** @return ban id, std::nullopt if unknown (bans inserted by BannedIpDAO::insert don't get their id) */
	std::optional<int32_t> getId() const noexcept { return id; }
	void setId(std::optional<int32_t> value) noexcept { id = value; }

	/** @return ip mask */
	const std::string& getMask() const noexcept { return mask; }
	void setMask(std::string value) noexcept { mask = std::move(value); }

	/** @return expiration time of ban, std::nullopt if the ban never expires */
	std::optional<commons::database::Timestamp> getTimeEnd() const noexcept { return timeEnd; }
	void setTimeEnd(std::optional<commons::database::Timestamp> value) noexcept { timeEnd = value; }

	/** Java: equals - true if this ip ban is equal to another, based on the mask */
	bool operator==(const BannedIP& other) const noexcept { return mask == other.mask; }

	/** Java: hashCode - based on the mask (Java String hash code) */
	int32_t hashCode() const;

private:
	/** id of ip ban */
	std::optional<int32_t> id;
	/** ip mask */
	std::string mask;
	/** expiration time */
	std::optional<commons::database::Timestamp> timeEnd;
};

} // namespace aion::loginserver::model

template <>
struct std::hash<aion::loginserver::model::BannedIP> {
	size_t operator()(const aion::loginserver::model::BannedIP& bannedIP) const noexcept { return std::hash<std::string>{}(bannedIP.getMask()); }
};
