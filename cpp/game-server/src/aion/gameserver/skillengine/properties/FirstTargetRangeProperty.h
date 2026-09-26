#pragma once

#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/skillengine/properties/Properties_CastState.h"
#include "aion/gameserver/skillengine/properties/fwd.h"

namespace aion::gameserver::skillengine::properties {

/**
 * Java com.aionemu.gameserver.skillengine.properties.FirstTargetRangeProperty: the range and line-of-sight check of the first target, at cast
 * start and again at cast end (with the revision distance for pvp targets and the weapon range when `awr` is set).
 * <p>
 * C++: a static-only class (Java never instantiates it); declarations of m5b2-plan.md S-03, the bodies are the cast lane's. The body calls
 * GeoService.canSee twice (the POINT arm and the final check), so this is where a geo run can differ from a geo-off run on the cast path
 * (m5b2-plan.md §13 question 2).
 *
 * @author ATracer
 */
class FirstTargetRangeProperty {
public:
	static bool set(model::Skill& skill, const Properties* properties, Properties_CastState castState);
};

} // namespace aion::gameserver::skillengine::properties
