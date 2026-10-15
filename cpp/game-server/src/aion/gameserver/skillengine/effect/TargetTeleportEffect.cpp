#include "aion/gameserver/skillengine/effect/TargetTeleportEffect.h"

#include <cmath>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillAliasLocationData.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillAliasLocation.h"
#include "aion/gameserver/skillengine/model/SkillAliasPosition.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

namespace {

/** Java Math.toRadians(double) (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
constexpr double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

} // namespace

// Java TargetTeleportEffect.java:33-53. `loc == null` is an empty string: the binder keeps an absent alias_location as "".
void TargetTeleportEffect::applyEffect(model::Effect& effect) const {
	using gameserver::model::gameobjects::Creature;
	using gameserver::model::gameobjects::player::Player;
	if (runtime::Ptr<Player> p = runtime::as<Player>(effect.getEffected())) {
		if (loc.empty()) { // teleport in front of effector
			const runtime::Ptr<Creature> effector = effect.isReflected() ? effect.getOriginalEffected() : effect.getEffector();
			double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(effector->getHeading()));
			float z = effector->getZ();
			const float x1 = static_cast<float>(std::cos(radian));
			const float y1 = static_cast<float>(std::sin(radian));
			geoEngine::math::Vector3f closestCollision =
				world::geo::GeoService::getInstance().getClosestCollision(*effect.getEffected(), effector->getX() + x1, effector->getY() + y1, z);
			services::teleport::TeleportService::teleportTo(*p, p->getWorldId(), closestCollision.getX(), closestCollision.getY(), closestCollision.getZ());
		} else { // teleport to random specified position
			const model::SkillAliasLocation* skillAliasLocation = dataholders::DataManager::SKILL_ALIAS_LOCATION_DATA->getSkillAliasLocation(loc);
			if (skillAliasLocation != nullptr && p->getWorldId() == skillAliasLocation->getWorldId()) {
				const model::SkillAliasPosition* aliasPosition = // Java: the local position (renamed: it would hide EffectTemplate.position)
					commons::utils::Rnd::get(skillAliasLocation->getSkillAliasPositionList());
				if (aliasPosition == nullptr) // Java: Rnd.get of an empty list is null, then position.getX()
					throw runtime::NullPointerException("skill alias location " + loc + " has no positions");
				services::teleport::TeleportService::teleportTo(*p, p->getWorldId(), aliasPosition->getX(), aliasPosition->getY(), aliasPosition->getZ());
			}
		}
	}
}

} // namespace aion::gameserver::skillengine::effect
