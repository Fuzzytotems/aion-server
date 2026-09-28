#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/MotionData.xml.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.MotionData.
 * <p>
 * C++: the index points into the bound `motionTimes` storage, which stays after afterUnmarshal (static-data.md §2.6). getMotionTimes returns
 * the motion times in Java's HashMap<String, MotionTime> iteration order. A motion without a name (Java null) has an empty name.
 *
 * @author kecimis
 */
class MotionData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MotionData.xml.inc"
private:
	std::map<std::string, const skillengine::model::MotionTime*, std::less<>> motionTimesMap;
	/** C++ only: motionTimesMap.values() in Java's HashMap iteration order */
	std::vector<const skillengine::model::MotionTime*> motionTimesInHashOrder;

public:
	/** Java record AnimationTimes(int lastHitMillis, int fullDurationMillis) */
	struct AnimationTimes {
		int32_t lastHitMillis;
		int32_t fullDurationMillis;
	};

	const std::vector<const skillengine::model::MotionTime*>& getMotionTimes() const;

	/** @return the motion time, nullptr (Java null) if there is none */
	const skillengine::model::MotionTime* getMotionTime(std::string_view name) const;

	/** @return the motion time of the skill's motion, nullptr (Java null) for skills without a (named) motion */
	const skillengine::model::MotionTime* getMotionTime(skillengine::model::Skill& skill) const;

	float calculateAnimationTimeUntilFirstHit(model::gameobjects::player::Player& player, skillengine::model::Skill& skill) const;

	/** @return the animation times, std::nullopt (Java null) if the skill has no motion time for the player */
	std::optional<AnimationTimes> calculateAnimationTimesAfterLastHit(model::gameobjects::player::Player& player,
	                                                                  skillengine::model::Skill& skill) const;

private:
	static float calculateCastSpeedRate(float castSpeedForAnimationBoost);

public:
	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
