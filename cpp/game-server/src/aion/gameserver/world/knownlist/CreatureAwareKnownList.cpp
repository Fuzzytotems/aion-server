#include "aion/gameserver/world/knownlist/CreatureAwareKnownList.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::world::knownlist {

CreatureAwareKnownList::CreatureAwareKnownList(model::gameobjects::VisibleObject& ownerValue) : KnownList(ownerValue) {
}

CreatureAwareKnownList::~CreatureAwareKnownList() = default;

bool CreatureAwareKnownList::isAwareOf(runtime::Ptr<model::gameobjects::VisibleObject> newObject) {
	return KnownList::isAwareOf(newObject) && runtime::as<model::gameobjects::Creature>(newObject);
}

} // namespace aion::gameserver::world::knownlist
