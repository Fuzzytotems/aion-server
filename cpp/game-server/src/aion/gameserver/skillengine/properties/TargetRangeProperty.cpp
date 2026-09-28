#include "aion/gameserver/skillengine/properties/TargetRangeProperty.h"

#include <cmath>
#include <vector>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/properties/AreaDirections.h"
#include "aion/gameserver/skillengine/properties/FirstTargetAttribute.h"
#include "aion/gameserver/skillengine/properties/TargetRangeAttribute.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::skillengine::properties {

using gameserver::model::gameobjects::AionObject;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Summon;
using gameserver::model::gameobjects::Trap;
using gameserver::model::gameobjects::VisibleObject;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::gameobjects::state::CreatureVisualState;
using gameserver::model::team::TemporaryPlayerTeam;
using runtime::Ptr;
using runtime::Ref;
using utils::PositionUtil;
using world::geo::GeoService;

bool TargetRangeProperty::set(const Properties* properties, Properties::ValidationResult& result, Creature& skillEffector,
	const model::SkillTemplate* skillTemplate, float x, float y, float z) {
	// Java: `TargetRangeAttribute value = properties.getTargetType()`; Properties.validateEffectedList runs this step only when target_type is set
	TargetRangeAttribute value = *properties->getTargetType();
	int32_t effectiveRange = runtime::as<Trap>(skillEffector) ? skillEffector.getGameStats()->getAttackRange()->getCurrent() : properties->getEffectiveRange();

	runtime::ArrayList<Ref<Creature>>& effectedList = result.getTargets();
	switch (value) {
		case TargetRangeAttribute::ONLYONE:
			break;
		case TargetRangeAttribute::AREA: {
			int32_t altitude = properties->getEffectiveAltitude() != 0 ? properties->getEffectiveAltitude() : 1;
			Ptr<Creature> firstTarget = result.getFirstTarget();

			if (!firstTarget)
				return false;

			// Create a sorted map of the objects in knownlist
			// and filter them properly
			// Java: one lazy stream; every filter runs per element in order, then forEach(effectedList::add)
			for (Ptr<world::knownlist::KnownObject> knownObject : firstTarget->getKnownList().stream()) {
				Ptr<Creature> creature = runtime::as<Creature>(knownObject->get());
				if (!creature)
					continue;
				if (!checkCommonRequirements(*creature, skillTemplate))
					continue;
				// .filter(creature -> !(creature instanceof Kisk && isInsideDisablePvpZone(creature))) is commented out in Java
				if (!(std::fabs(firstTarget->getZ() - creature->getZ()) <= static_cast<float>(altitude)))
					continue;
				if (Ptr<Player> player = runtime::as<Player>(creature); player && player->isUsingFlightTransporterOrWindstream())
					continue;
				if (Ptr<Trap> trap = runtime::as<Trap>(skillEffector); trap && trap->getCreator() == creature) // TODO this is a temporary hack for traps
					continue;
				if (!checkRange(properties, skillEffector, x, y, z, *creature, effectiveRange, *firstTarget))
					continue;
				if (!checkGeo(*creature, result.getFirstTarget(), skillTemplate))
					continue;
				effectedList.add(Ref<Creature>(creature));
			}
			break;
		}
		case TargetRangeAttribute::PARTY:
		case TargetRangeAttribute::PARTY_WITHPET: {
			// if only firsttarget will be affected (e.g. Bodyguard), we don't need to evaluate the whole group
			if (properties->getTargetMaxCount() == 1 && properties->getFirstTarget() != FirstTargetAttribute::POINT)
				break;
			if (Ptr<Player> effector = runtime::as<Player>(skillEffector)) {
				Ptr<TemporaryPlayerTeam> team;
				if (value == TargetRangeAttribute::PARTY_WITHPET) {
					team = effector->getCurrentTeam(); // group or whole alliance
					tryAddSummon(effector->getSummon(), result, skillTemplate, effectedList);
				} else {
					team = effector->getCurrentGroup(); // group or alliance group (max 6 targets)
				}
				if (team) {
					effectedList.clear();
					for (Ptr<AionObject> memberObject : team->getMembers()) {
						Ptr<Player> member = runtime::cast<Player>(memberObject); // Java: List<Player> of a TemporaryPlayerTeam
						if (!member->isOnline())
							continue;
						if (!checkCommonRequirements(*member, skillTemplate))
							continue;
						if (PositionUtil::isInRange(*effector, *member, static_cast<float>(effectiveRange), false)) {
							if (checkGeo(*member, result.getFirstTarget(), skillTemplate))
								effectedList.add(Ref<Creature>(member));
							if (value == TargetRangeAttribute::PARTY_WITHPET)
								tryAddSummon(member->getSummon(), result, skillTemplate, effectedList);
						}
					}
				}
			}
			break;
		}
		case TargetRangeAttribute::POINT:
			for (Ptr<world::knownlist::KnownObject> knownObject : skillEffector.getKnownList().stream()) {
				Ptr<Creature> creature = runtime::as<Creature>(knownObject->get());
				if (!creature)
					continue;
				if (!checkCommonRequirements(*creature, skillTemplate))
					continue;
				if (Ptr<Trap> trap = runtime::as<Trap>(creature); trap && !trap->getMaster()->isEnemy(skillEffector))
					continue;
				if (!PositionUtil::isInRange(*creature, x, y, z, static_cast<float>(properties->getTargetDistance() + 1)))
					continue;
				if (!checkGeo(*creature, result.getFirstTarget(), skillTemplate))
					continue;
				effectedList.add(Ref<Creature>(creature));
			}
			break;
		default:
			break;
	}

	return true;
}

bool TargetRangeProperty::checkCommonRequirements(Creature& creature, const model::SkillTemplate* skillTemplate) {
	if (skillTemplate->hasResurrectEffect()) {
		if (!creature.isDead())
			return false;
	} else {
		if (creature.isDead())
			return false;
	}

	// blinking state means protection is active (no interaction with creature is possible)
	if (creature.isInVisualState(CreatureVisualState::BLINKING))
		return false;

	return true;
}

bool TargetRangeProperty::isInsideDisablePvpZone(Creature& creature) {
	if (creature.isInsideZoneType(gameserver::model::templates::zone::ZoneType::PVP)) {
		for (Ptr<world::zone::ZoneInstance> zone : creature.findZones()) {
			if (zone->getZoneTemplate()->getFlags() == 0)
				return true;
		}
	}
	return false;
}

bool TargetRangeProperty::checkRange(const Properties* properties, Creature& skillEffector, float x, float y, float z, Creature& creature,
	int32_t effectiveRange, Creature& firstTarget) {
	if (properties->getFirstTarget() == FirstTargetAttribute::POINT)
		return PositionUtil::isInRange(x, y, z, creature.getX(), creature.getY(), creature.getZ(), static_cast<float>(effectiveRange));
	if (properties->getIneffectiveRange() > 0 && PositionUtil::isInRange(firstTarget, creature, static_cast<float>(properties->getIneffectiveRange()), false))
		return false;
	if (properties->getEffectiveDist() > 0) {
		if (properties->getEffectiveAngle() > 0) {
			if (creature.equals(skillEffector))
				return false;
			// for target_range_area_type = firestorm
			if (properties->getEffectiveAngle() < 360) {
				// e.g. 60 degrees (always positive) = 30 degrees in positive and negative direction
				float angle = static_cast<float>(properties->getEffectiveAngle()) / 2.0f;
				if (properties->getDirection() == AreaDirections::BACK) {
					if (!PositionUtil::isBehind(creature, skillEffector, angle))
						return false;
				} else if (!PositionUtil::isInFrontOf(creature, skillEffector, angle)) {
					return false;
				}
			}
			return PositionUtil::isInRange(skillEffector, creature, static_cast<float>(properties->getEffectiveDist()), false);
		} else {
			// Lightning bolt
			return PositionUtil::isInsideAttackCylinder(skillEffector, creature, static_cast<float>(properties->getEffectiveDist()),
				static_cast<float>(effectiveRange) / 2.0f, properties->getDirection());
		}
	}
	return PositionUtil::isInRange(firstTarget, creature, static_cast<float>(effectiveRange), false);
}

bool TargetRangeProperty::checkGeo(VisibleObject& object, Ptr<Creature> firstTarget, const model::SkillTemplate* skillTemplate) {
	// If creature is at least 2 meters above the terrain, ground skill cannot be applied
	if (configs::main::GeoDataConfig::GEO_ENABLE) {
		if (skillTemplate->isGroundSkill()) {
			float geoZ = GeoService::getInstance().getZ(object, object.getZ() + 2, object.getZ() - 2);
			if (std::isnan(geoZ))
				return false;
		}
		// Java dereferences a null first target here (NullPointerException); Ptr's operator* throws the same
		if (skillTemplate->getProperties()->getFirstTarget() != FirstTargetAttribute::POINT && !GeoService::getInstance().canSee(*firstTarget, object))
			return false;
	}
	return true;
}

void TargetRangeProperty::tryAddSummon(Ptr<Summon> summon, Properties::ValidationResult& result, const model::SkillTemplate* skillTemplate,
	runtime::ArrayList<Ref<Creature>>& effectedList) {
	if (summon && checkGeo(*summon, result.getFirstTarget(), skillTemplate))
		effectedList.add(Ref<Creature>(summon));
}

} // namespace aion::gameserver::skillengine::properties
