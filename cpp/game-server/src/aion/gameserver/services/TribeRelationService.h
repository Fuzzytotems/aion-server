#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Cheatkiller
 */
class TribeRelationService {
public:
	static bool isAggressive(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2);
	static bool isFriend(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2);
	static bool isSupport(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2);
	static bool isNone(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2);
	static bool isNeutral(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2);
	static bool isHostile(model::gameobjects::Creature& creature1, model::gameobjects::Creature& creature2);
	static bool checkSiegeRelation(model::gameobjects::Npc& npc, model::gameobjects::Creature& creature);
	static bool canHelpCreature(model::gameobjects::Creature& creature, model::gameobjects::Creature& creatureAskingForSupport);
};

} // namespace aion::gameserver::services
