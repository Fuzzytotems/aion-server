#include "aion/gameserver/dataholders/EventData.h"

#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void EventData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: if (events == null) events = Collections.emptyList(); (the C++ list is empty then)
	for (const model::templates::event::EventTemplate& ev : events) {
		if (ev.getEndDate() && ev.getStartDate() && !(*ev.getStartDate() < *ev.getEndDate()))
			ctx.fail("Event \"" + ev.getName() + "\" has an invalid start or end date: start date must be before end date");
	}
}

int32_t EventData::size() const {
	return static_cast<int32_t>(events.size());
}

void EventData::setEvents(std::vector<model::templates::event::EventTemplate> /*events*/) {
	AION_UNPORTED(); // //reload of static data is deferred (design D3): a published holder is immutable
}

void EventData::addAllNpcIdsToSet(std::unordered_set<int32_t>& npcIds) const {
	for (const model::templates::event::EventTemplate& ev : events) {
		if (ev.getSpawns() != nullptr)
			ev.getSpawns()->addAllNpcIdsToSet(npcIds);
	}
}

} // namespace aion::gameserver::dataholders
