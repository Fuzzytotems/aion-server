#include "aion/gameserver/runtime/sched/Clock.h"

namespace aion::gameserver::runtime {

const SystemClock& SystemClock::getInstance() noexcept {
	static const SystemClock instance;
	return instance;
}

int64_t SystemClock::currentTimeMillis() const noexcept {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace aion::gameserver::runtime
