#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "aion/commons/database/SqlTypes.h"

namespace aion::loginserver::model::base {

/**
 * A banned MAC address with its ban end time and details.
 * <p>
 * A value type (not synchronized).
 * <p>
 * Java: com.aionemu.loginserver.model.base.BannedMacEntry
 *
 * @author KID
 */
class BannedMacEntry {
public:
	/**
	 * @param address the MAC address
	 * @param newTime ban end in milliseconds since the epoch
	 * Deviation: details are empty (Java: null, which BannedMacDAO::update could not store in the NOT NULL column).
	 */
	BannedMacEntry(std::string address, int64_t newTime);

	BannedMacEntry(std::string address, std::optional<commons::database::Timestamp> time, std::string details);

	void setDetails(std::string value) { details = std::move(value); }

	/** Sets the ban end to newTime milliseconds since the epoch */
	void updateTime(int64_t newTime) noexcept;

	const std::string& getMac() const noexcept { return mac; }

	/** @return the ban end, std::nullopt if unset (never the case for entries created by the constructors or loaded from the database) */
	std::optional<commons::database::Timestamp> getTime() const noexcept { return timeEnd; }

	/** @return true if the ban end is set and lies in the future */
	bool isActive() const noexcept;

	const std::string& getDetails() const noexcept { return details; }

private:
	std::string mac;
	std::string details;
	std::optional<commons::database::Timestamp> timeEnd;
};

} // namespace aion::loginserver::model::base
