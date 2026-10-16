#include "aion/gameserver/model/templates/quest/QuestDrop.h"

namespace aion::gameserver::model::templates::quest {

void QuestDrop::setQuestId(std::optional<int32_t> value) {
	this->questId = value;
}

} // namespace aion::gameserver::model::templates::quest
