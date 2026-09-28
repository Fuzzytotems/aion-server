#include "aion/gameserver/custom/instance/CustomInstanceRankedPlayer.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::custom::instance {

CustomInstanceRankedPlayer::CustomInstanceRankedPlayer(int32_t value, int32_t rankValue, int64_t lastEntryValue, int32_t maxRankValue,
	int32_t dpsValue, std::string_view nameValue, model::PlayerClass playerClassValue)
	: CustomInstanceRank(value, rankValue, lastEntryValue, maxRankValue, dpsValue), name(std::string(nameValue)), playerClass(playerClassValue) {
}

} // namespace aion::gameserver::custom::instance
