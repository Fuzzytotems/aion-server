#include "aion/gameserver/model/templates/event/EventQuestList.h"

namespace aion::gameserver::model::templates::event {

namespace {
const std::vector<int32_t> EMPTY; // Java Collections.emptyList()
} // namespace

const std::vector<int32_t>& EventQuestList::getStartableQuests() const {
	return startQuests ? *startQuests : EMPTY;
}

const std::vector<int32_t>& EventQuestList::getMaintainQuests() const {
	return maintainQuests ? *maintainQuests : EMPTY;
}

} // namespace aion::gameserver::model::templates::event
