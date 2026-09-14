#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::world::knownlist {

PlayerAwareKnownList::PlayerAwareKnownList(model::gameobjects::VisibleObject& ownerValue) : KnownList(ownerValue) {
}

PlayerAwareKnownList::~PlayerAwareKnownList() = default;

bool PlayerAwareKnownList::isAwareOf(runtime::Ptr<model::gameobjects::VisibleObject> newObject) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::knownlist
