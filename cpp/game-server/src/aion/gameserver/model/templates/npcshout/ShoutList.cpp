#include "aion/gameserver/model/templates/npcshout/ShoutList.h"

namespace aion::gameserver::model::templates::npcshout {

const std::vector<int32_t>& ShoutList::getNpcIds() const {
	static const std::vector<int32_t> empty;
	return npcIds ? *npcIds : empty;
}

void ShoutList::makeNull() {
	this->npcIds.reset();
	this->npcShouts.clear();
	this->restrictWorld.reset();
}

} // namespace aion::gameserver::model::templates::npcshout
