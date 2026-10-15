#include "aion/gameserver/model/account/AccountTime.h"

namespace aion::gameserver::model::account {

AccountTime::AccountTime() = default;

AccountTime::~AccountTime() = default;

runtime::Ref<AccountTime> AccountTime::create() {
	return runtime::makeRef<AccountTime>();
}

int32_t AccountTime::getAccumulatedOnlineHours() {
	return toHours(accumulatedOnlineTime.get());
}

int32_t AccountTime::getAccumulatedOnlineMinutes() {
	return toMinutes(accumulatedOnlineTime.get());
}

int32_t AccountTime::getAccumulatedRestHours() {
	return toHours(accumulatedRestTime.get());
}

int32_t AccountTime::getAccumulatedRestMinutes() {
	return toMinutes(accumulatedRestTime.get());
}

int32_t AccountTime::toHours(int64_t millis) {
	// Java: (int) (millis / 1000) / 3600 (the cast narrows before the division)
	return static_cast<int32_t>(millis / 1000) / 3600;
}

int32_t AccountTime::toMinutes(int64_t millis) {
	// Java: (int) ((millis / 1000) % 3600) / 60
	return static_cast<int32_t>((millis / 1000) % 3600) / 60;
}

} // namespace aion::gameserver::model::account
