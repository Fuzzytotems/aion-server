#include "aion/gameserver/model/account/AccountTime.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::account {

AccountTime::AccountTime() = default;

AccountTime::~AccountTime() = default;

runtime::Ref<AccountTime> AccountTime::create() {
	return runtime::makeRef<AccountTime>();
}

int32_t AccountTime::getAccumulatedOnlineHours() {
	AION_UNPORTED();
}

int32_t AccountTime::getAccumulatedOnlineMinutes() {
	AION_UNPORTED();
}

int32_t AccountTime::getAccumulatedRestHours() {
	AION_UNPORTED();
}

int32_t AccountTime::getAccumulatedRestMinutes() {
	AION_UNPORTED();
}

int32_t AccountTime::toHours(int64_t millis) {
	AION_UNPORTED();
}

int32_t AccountTime::toMinutes(int64_t millis) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::account
