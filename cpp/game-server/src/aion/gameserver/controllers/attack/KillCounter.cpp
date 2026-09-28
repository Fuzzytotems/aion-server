#include "aion/gameserver/controllers/attack/KillCounter.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::controllers::attack {

using runtime::Ptr;
using runtime::Ref;

int32_t KillCounter::addKillFor(int32_t killerId, int32_t victimId) {
	int64_t now = commons::utils::currentTimeMillis();
	int64_t minAge = now - configs::main::CustomConfig::PVP_DAY_DURATION.load();
	Ptr<runtime::RcHashMap<int32_t, Ref<runtime::RcArrayList<int64_t>>>> killTimesByVictimId = PVP_KILL_LISTS.computeIfAbsent(
		killerId, [] { return runtime::RcHashMap<int32_t, Ref<runtime::RcArrayList<int64_t>>>::create(); });
	SYNCHRONIZED(*killTimesByVictimId) {
		Ptr<runtime::RcArrayList<int64_t>> killTimes =
			killTimesByVictimId->computeIfAbsent(victimId, [] { return runtime::RcArrayList<int64_t>::create(); });
		killTimes->removeIf([minAge](int64_t time) { return time < minAge; });
		killTimes->add(now);
		return killTimes->size();
	}
}

} // namespace aion::gameserver::controllers::attack
