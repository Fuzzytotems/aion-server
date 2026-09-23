#pragma once

#include <vector>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/fwd.h"

namespace aion::gameserver::skillengine::properties {

/**
 * Java com.aionemu.gameserver.skillengine.properties.TargetStatusProperty: keeps the targets that are in one of the template's
 * `target_status` abnormal states; the cast fails if the first target is filtered out.
 * <p>
 * C++: a static-only class (Java never instantiates it; its JAXB annotations bind nothing, so xmlgen writes no shell for it); declarations of
 * m5b2-plan.md S-03, the bodies are the cast lane's. It includes Properties.h for the nested ValidationResult (hub-headers.md §9.3).
 * hasAnyAbnormalState's `states` is `*properties->getTargetStatus()`, which Properties.validateEffectedList only reaches when it is set.
 *
 * @author kecimis
 */
class TargetStatusProperty {
public:
	static bool set(const Properties* properties, Properties::ValidationResult& result, const model::SkillTemplate* skillTemplate);

private:
	static bool hasAnyAbnormalState(gameserver::model::gameobjects::Creature& creature, const std::vector<effect::AbnormalState>& states);
};

} // namespace aion::gameserver::skillengine::properties
