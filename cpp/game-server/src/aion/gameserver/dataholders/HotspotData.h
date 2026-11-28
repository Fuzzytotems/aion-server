#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/HotspotData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.HotspotData.
 * <p>
 * C++: an absent list is the empty bound vector; Java's size and getHotspotTemplateById throw NullPointerException then (upstream 15eaf77b6
 * removed the lazy empty list and getHotspotTemplates). The shipped data has hotspots.
 *
 * @author ginho1
 */
class HotspotData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HotspotData.xml.inc"
public:
	int32_t size() const;

	/** @return the first hotspot with the id, nullptr (Java null) if there is none */
	const model::templates::hotspot::HotspotTemplate* getHotspotTemplateById(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
