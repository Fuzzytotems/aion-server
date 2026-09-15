#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/WorldMapsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Object of this class is containing <tt>WorldMapTemplate</tt> objects for all world maps. World maps are defined in
 * data/static_data/world_maps.xml file.
 * <p>
 * C++: the @XmlTransient index points into the bound `worldMaps` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the
 * list to null). Declarations beyond Java's getTemplate, and the LinkedHashMap order they need, come with the P4-09 port (header request pre-2).
 *
 * @author Luno
 */
class WorldMapsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WorldMapsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::world::WorldMapTemplate*> mapsById;

public:
	/** @return the map template, nullptr (Java null) if there is none */
	const model::templates::world::WorldMapTemplate* getTemplate(int32_t worldId) const;
};

} // namespace aion::gameserver::dataholders
