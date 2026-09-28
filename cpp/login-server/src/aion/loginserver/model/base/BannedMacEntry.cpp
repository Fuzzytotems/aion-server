#include "aion/loginserver/model/base/BannedMacEntry.h"

#include <chrono>
#include <utility>

#include "aion/loginserver/model/AccountTime.h"

namespace aion::loginserver::model::base {

using commons::database::Timestamp;

BannedMacEntry::BannedMacEntry(std::string address, int64_t newTime) : mac(std::move(address)) {
	updateTime(newTime);
}

BannedMacEntry::BannedMacEntry(std::string address, std::optional<Timestamp> time, std::string details)
	: mac(std::move(address)), details(std::move(details)), timeEnd(time) {
}

void BannedMacEntry::updateTime(int64_t newTime) noexcept {
	timeEnd = Timestamp(std::chrono::milliseconds(newTime));
}

bool BannedMacEntry::isActive() const noexcept {
	return timeEnd && *timeEnd > currentTimestamp();
}

} // namespace aion::loginserver::model::base
