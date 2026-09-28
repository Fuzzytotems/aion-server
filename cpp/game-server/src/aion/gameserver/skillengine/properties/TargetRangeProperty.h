#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/fwd.h"

namespace aion::gameserver::skillengine::properties {

/**
 * Java com.aionemu.gameserver.skillengine.properties.TargetRangeProperty: adds the creatures a skill of `target_type` ONLYONE, AREA, PARTY,
 * PARTY_WITHPET or POINT reaches to the effected list.
 * <p>
 * C++: a static-only class (Java never instantiates it); declarations of m5b2-plan.md S-03, the bodies are the cast lane's. It includes
 * Properties.h for the nested ValidationResult (hub-headers.md §9.3).
 * - checkGeo calls GeoService.getZ and canSee when geodata is enabled (GeoDataConfig.GEO_ENABLE), so the AREA, PARTY and POINT arms are cast
 *   path code that a geo run exercises and a geo-off run does not (m5b2-plan.md §13 question 2, item G-04).
 * - checkGeo's `firstTarget` is `result.getFirstTarget()`, nullable; checkRange's is the AREA arm's local after its null check.
 * - tryAddSummon's `summon` is nullable (Java checks it; Player.getSummon may be null) and `effectedList` is `result.getTargets()`, the list
 *   it fills.
 * - isInsideDisablePvpZone is unused in Java (`@SuppressWarnings("unused")`, its only call is commented out) and declared all the same.
 *
 * @author ATracer, Yeats, Neon
 */
class TargetRangeProperty {
public:
	static bool set(const Properties* properties, Properties::ValidationResult& result, gameserver::model::gameobjects::Creature& skillEffector,
		const model::SkillTemplate* skillTemplate, float x, float y, float z);

private:
	static bool checkCommonRequirements(gameserver::model::gameobjects::Creature& creature, const model::SkillTemplate* skillTemplate);

	static bool isInsideDisablePvpZone(gameserver::model::gameobjects::Creature& creature);

	static bool checkRange(const Properties* properties, gameserver::model::gameobjects::Creature& skillEffector, float x, float y, float z,
		gameserver::model::gameobjects::Creature& creature, int32_t effectiveRange, gameserver::model::gameobjects::Creature& firstTarget);

	static bool checkGeo(gameserver::model::gameobjects::VisibleObject& object, runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget,
		const model::SkillTemplate* skillTemplate);

	static void tryAddSummon(runtime::Ptr<gameserver::model::gameobjects::Summon> summon, Properties::ValidationResult& result,
		const model::SkillTemplate* skillTemplate, runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& effectedList);
};

} // namespace aion::gameserver::skillengine::properties
