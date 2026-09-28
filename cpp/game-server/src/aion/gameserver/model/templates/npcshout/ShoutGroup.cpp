#include "aion/gameserver/model/templates/npcshout/ShoutGroup.h"

namespace aion::gameserver::model::templates::npcshout {

void ShoutGroup::makeNull() {
	this->shoutNpcs.clear();
	this->clientAi.clear();
}

} // namespace aion::gameserver::model::templates::npcshout
