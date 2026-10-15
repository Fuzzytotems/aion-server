#include "aion/gameserver/custom/instance/CustomInstanceRank.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::custom::instance {

CustomInstanceRank::CustomInstanceRank(int32_t value, int32_t rankValue, int64_t lastEntryValue, int32_t maxRankValue, int32_t dpsValue)
	: playerId(value), rank(rankValue), maxRank(maxRankValue), dps(dpsValue), lastEntry(lastEntryValue) {
}

} // namespace aion::gameserver::custom::instance
