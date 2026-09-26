#include "aion/gameserver/world/knownlist/FlagKnownList.h"

#include <limits>
#include <vector>

#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::world::knownlist {

namespace {

/** Java: `if (!owner.isFlag()) throw new IllegalArgumentException();` before `super(owner)` */
model::gameobjects::Npc& checkedFlag(model::gameobjects::Npc& owner) {
	if (!owner.isFlag())
		throw runtime::IllegalArgumentException("");
	return owner;
}

} // namespace

FlagKnownList::FlagKnownList(model::gameobjects::Npc& ownerValue) : PlayerAwareKnownList(checkedFlag(ownerValue)) {
}

FlagKnownList::~FlagKnownList() = default;

void FlagKnownList::update() {
	SYNCHRONIZED(*this) {
		runtime::Ptr<WorldMapInstance> worldMapInstance = owner.getPosition()->getWorldMapInstance();
		// Java: knownObjects.values().removeIf(knownObject -> knownObject.get().getWorldMapInstance() != worldMapInstance);
		// Deviation (class comment): both edges are removed, under the pair lock and without notifications like removeIf
		for (runtime::Ptr<KnownObject> knownObject : knownObjects.values()) {
			if (knownObject->get()->getWorldMapInstance() != worldMapInstance) {
				delPair(owner, *knownObject->get());
			}
		}
		worldMapInstance->forEachPlayer([this](model::gameobjects::player::Player& player) {
			addPair(owner, player); // Java: if (player.getKnownList().add(owner)) add(player);
		});
	}
}

float FlagKnownList::getVisibleDistance() {
	return std::numeric_limits<float>::max(); // Java: Float.MAX_VALUE
}

} // namespace aion::gameserver::world::knownlist
