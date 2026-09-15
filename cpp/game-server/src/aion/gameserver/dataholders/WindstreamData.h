#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/WindstreamData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.WindstreamData.
 * <p>
 * C++: the index points into the bound `wts` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author LokiReborn
 */
class WindstreamData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WindstreamData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::windstreams::WindstreamTemplate*> windstreams;

public:
	/** @return the windstreams of the map, nullptr (Java null) if there are none */
	const model::templates::windstreams::WindstreamTemplate* getStreamTemplate(int32_t mapId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
