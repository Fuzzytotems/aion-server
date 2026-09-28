#include "aion/gameserver/model/templates/quest/QuestKill.h"

namespace aion::gameserver::model::templates::quest {

const std::vector<int32_t>& QuestKill::getNpcIds() const {
	static const std::vector<int32_t> empty;
	return npcIds ? *npcIds : empty;
}

} // namespace aion::gameserver::model::templates::quest
