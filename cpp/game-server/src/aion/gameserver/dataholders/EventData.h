#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dataholders/EventData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.EventData.
 * <p>
 * C++: an absent event list is the empty bound vector (Java replaces null with an empty list). An invalid start/end date fails the load through
 * LoadContext::fail with Java's IllegalArgumentException message. setEvents serves only the //reload command, which is deferred (design D3).
 */
class EventData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/EventData.xml.inc"
public:
	int32_t size() const;

	/** Java: replaces the events and runs afterUnmarshal again (//reload only; deferred, design D3) */
	void setEvents(std::vector<model::templates::event::EventTemplate> events);

	void addAllNpcIdsToSet(std::unordered_set<int32_t>& npcIds) const;
};

} // namespace aion::gameserver::dataholders
