#include "aion/gameserver/model/PlayerClassInfo.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model {

const templates::stats::StatsTemplate* createStatsTemplate(PlayerClass playerClass, int32_t level) {
	// Java: new PlayerStatsTemplate() with the PlayerStatCalculator values (P5-01) and the class' base attributes (see PlayerClassInfo.h)
	AION_UNPORTED();
}

std::optional<PlayerClass> getPlayerClassById(int8_t classId, bool ignoreInvalidClassId) {
	for (size_t i = 0; i < detail::PLAYER_CLASS_DATA.size(); ++i) {
		if (detail::PLAYER_CLASS_DATA[i].classId == classId)
			return static_cast<PlayerClass>(i);
	}
	if (ignoreInvalidClassId)
		return std::nullopt;
	throw runtime::IllegalArgumentException("There is no player class with id " + std::to_string(classId));
}

PlayerClass getPlayerClassById(int8_t classId) {
	return *getPlayerClassById(classId, false);
}

} // namespace aion::gameserver::model
