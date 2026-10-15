#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::world::knownlist {

NpcKnownList::NpcKnownList(model::gameobjects::VisibleObject& ownerValue) : CreatureAwareKnownList(ownerValue) {
}

NpcKnownList::~NpcKnownList() = default;

void NpcKnownList::update() {
	if (owner.getPosition()->isMapRegionActive())
		KnownList::update();
	else
		clear(model::animations::ObjectDeleteAnimation::FADE_OUT);
}

} // namespace aion::gameserver::world::knownlist
