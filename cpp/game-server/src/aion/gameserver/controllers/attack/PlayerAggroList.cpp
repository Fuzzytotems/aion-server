#include "aion/gameserver/controllers/attack/PlayerAggroList.h"

#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::controllers::attack {

PlayerAggroList::PlayerAggroList(model::gameobjects::Creature& ownerValue) : AggroList(ownerValue) {
}

PlayerAggroList::~PlayerAggroList() = default;

bool PlayerAggroList::isAware(runtime::Ptr<model::gameobjects::Creature> creature) {
	return creature && owner.getKnownList().knows(*creature);
}

} // namespace aion::gameserver::controllers::attack
