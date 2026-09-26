#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/fwd.h"

namespace aion::gameserver::skillengine::properties {

/**
 * Java com.aionemu.gameserver.skillengine.properties.TargetRelationProperty: filters the effected list by `target_relation` (ALL, ENEMY,
 * FRIEND, MYPARTY) and moves the first target for FRIEND and MYPARTY.
 * <p>
 * C++: a static-only class (Java never instantiates it); declarations of m5b2-plan.md S-03, the bodies are the cast lane's. It includes
 * Properties.h for the nested ValidationResult (hub-headers.md §9.3). isBuffAllowed's parameters are nullable (its body checks both for null,
 * hub-headers.md §5.1 rule 2).
 *
 * @author ATracer
 */
class TargetRelationProperty {
public:
	static bool set(const Properties* properties, Properties::ValidationResult& result, gameserver::model::gameobjects::Creature& effector,
		const model::SkillTemplate* skillTemplate);

	/** @return true = allow buff, false = deny buff */
	static bool isBuffAllowed(runtime::Ptr<gameserver::model::gameobjects::Creature> source,
		runtime::Ptr<gameserver::model::gameobjects::Creature> target);

	static bool isSameAreaType(gameserver::model::gameobjects::Creature& source, gameserver::model::gameobjects::Creature& target);
};

} // namespace aion::gameserver::skillengine::properties
