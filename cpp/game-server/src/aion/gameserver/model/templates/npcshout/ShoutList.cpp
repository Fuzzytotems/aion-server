#include "aion/gameserver/model/templates/npcshout/ShoutList.h"

namespace aion::gameserver::model::templates::npcshout {

const std::vector<int32_t>& ShoutList::getNpcIds() const {
	static const std::vector<int32_t> empty;
	return npcIds ? *npcIds : empty;
}

} // namespace aion::gameserver::model::templates::npcshout
