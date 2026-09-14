#include "aion/gameserver/utils/time/gametime/GameTime.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::utils::time::gametime {

GameTime::GameTime(std::optional<int32_t> time) {
	// Java: if (time != null && time < 0) throw new IllegalArgumentException("Time must be >= 0"); gameTime = time == null ? 0 : time; dayTime =
	// calculateDayTime()
	AION_UNPORTED();
}

runtime::Ref<GameTime> GameTime::create(std::optional<int32_t> time) {
	return runtime::makeRef<GameTime>(time);
}

void GameTime::addMinutes(int32_t minutes) {
	AION_UNPORTED();
}

bool GameTime::setDayTime(DayTime value) {
	AION_UNPORTED();
}

int32_t GameTime::getYear() {
	AION_UNPORTED();
}

int32_t GameTime::getMonth() {
	AION_UNPORTED();
}

int32_t GameTime::getDay() {
	AION_UNPORTED();
}

int32_t GameTime::getHour() {
	AION_UNPORTED();
}

int32_t GameTime::getMinute() {
	AION_UNPORTED();
}

void GameTime::onHourChange(bool changedByClock) {
	AION_UNPORTED();
}

DayTime GameTime::calculateDayTime() {
	AION_UNPORTED();
}

runtime::Ref<GameTime> GameTime::minus(GameTime& gt) {
	AION_UNPORTED();
}

runtime::Ref<GameTime> GameTime::plus(GameTime& gt) {
	AION_UNPORTED();
}

bool GameTime::isGreaterThan(GameTime& gt) {
	AION_UNPORTED();
}

bool GameTime::isLessThan(GameTime& gt) {
	AION_UNPORTED();
}

int32_t GameTime::hashCode() const {
	AION_UNPORTED();
}

bool GameTime::equals(const GameTime& obj) const {
	AION_UNPORTED();
}

runtime::Ref<GameTime> GameTime::clone() {
	AION_UNPORTED();
}

GameTime::~GameTime() = default;

} // namespace aion::gameserver::utils::time::gametime
