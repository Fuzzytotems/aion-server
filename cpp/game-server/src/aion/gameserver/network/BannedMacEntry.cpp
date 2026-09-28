#include "aion/gameserver/network/BannedMacEntry.h"

#include <chrono>

#include "aion/commons/utils/TimeUtils.h"

namespace aion::gameserver::network {

namespace {

/** Java: new Timestamp(millis) */
commons::database::Timestamp timestampOf(int64_t millis) {
	return commons::database::Timestamp(std::chrono::milliseconds(millis));
}

/** Java: Timestamp.getTime() */
int64_t millisOf(const commons::database::Timestamp& timestamp) {
	return timestamp.time_since_epoch().count();
}

} // namespace

BannedMacEntry::BannedMacEntry(std::string_view address, int64_t newTime) : mac(address) {
	this->updateTime(newTime);
}

BannedMacEntry::BannedMacEntry(std::string_view address, std::optional<commons::database::Timestamp> time, std::string_view detailsValue)
	: mac(address), details(std::string(detailsValue)) {
	this->timeEnd.set(time);
}

BannedMacEntry::~BannedMacEntry() = default;

runtime::Ref<BannedMacEntry> BannedMacEntry::create(std::string_view address, int64_t newTime) {
	return runtime::makeRef<BannedMacEntry>(address, newTime);
}

runtime::Ref<BannedMacEntry> BannedMacEntry::create(std::string_view address, std::optional<commons::database::Timestamp> time,
	std::string_view detailsValue) {
	return runtime::makeRef<BannedMacEntry>(address, time, detailsValue);
}

void BannedMacEntry::updateTime(int64_t newTime) {
	this->timeEnd.set(timestampOf(newTime));
}

bool BannedMacEntry::isActive() {
	std::optional<commons::database::Timestamp> end = timeEnd.get();
	return end && millisOf(*end) > commons::utils::currentTimeMillis();
}

bool BannedMacEntry::isActiveTill(int64_t time) {
	std::optional<commons::database::Timestamp> end = timeEnd.get();
	return end && millisOf(*end) > time;
}

} // namespace aion::gameserver::network
