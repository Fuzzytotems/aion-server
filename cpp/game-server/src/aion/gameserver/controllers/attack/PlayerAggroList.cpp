#include "aion/gameserver/controllers/attack/PlayerAggroList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::attack {

PlayerAggroList::PlayerAggroList(model::gameobjects::Creature& ownerValue) : AggroList(ownerValue) {
}

PlayerAggroList::~PlayerAggroList() = default;

bool PlayerAggroList::isAware(runtime::Ptr<model::gameobjects::Creature> creature) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::attack
