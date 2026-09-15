#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"

namespace aion::gameserver::controllers::observer {

AttackerCriticalStatus::AttackerCriticalStatus(bool resultValue) : result(resultValue), value(0), isPercent_(false) {
}

AttackerCriticalStatus::AttackerCriticalStatus(int32_t countValue, int32_t valueValue, bool isPercentValue)
	: count(countValue), value(valueValue), isPercent_(isPercentValue) {
}

AttackerCriticalStatus::~AttackerCriticalStatus() = default;

runtime::Ref<AttackerCriticalStatus> AttackerCriticalStatus::create(bool resultValue) {
	return runtime::makeRef<AttackerCriticalStatus>(resultValue);
}

runtime::Ref<AttackerCriticalStatus> AttackerCriticalStatus::create(int32_t countValue, int32_t valueValue, bool isPercentValue) {
	return runtime::makeRef<AttackerCriticalStatus>(countValue, valueValue, isPercentValue);
}

} // namespace aion::gameserver::controllers::observer
