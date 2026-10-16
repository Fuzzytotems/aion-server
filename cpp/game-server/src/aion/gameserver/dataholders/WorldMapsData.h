#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/WorldMapsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Object of this class is containing <tt>WorldMapTemplate</tt> objects for all world maps. World maps are defined in
 * data/static_data/world_maps.xml file.
 * <p>
 * C++: the @XmlTransient index points into the bound `worldMaps` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the
 * list to null). Java's LinkedHashMap iteration order (a repeated map id keeps its first position with the last template) is the C++-only
 * `mapsInOrder`, which begin()/end() (Java Iterable) walk.
 *
 * @author Luno
 */
class WorldMapsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WorldMapsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::world::WorldMapTemplate*> mapsById;
	/** C++ only: mapsById.values() in Java's LinkedHashMap iteration order (map ids, resolved through mapsById) */
	std::vector<int32_t> mapIdsInOrder;
	std::vector<const model::templates::world::WorldMapTemplate*> mapsInOrder;

public:
	/** Java iterator(): the templates in insertion order */
	std::vector<const model::templates::world::WorldMapTemplate*>::const_iterator begin() const;
	std::vector<const model::templates::world::WorldMapTemplate*>::const_iterator end() const;

	/** Java: mapsById.values().parallelStream().forEach(consumer) on the ForkJoin common pool */
	void forEachParalllel(const std::function<void(const model::templates::world::WorldMapTemplate&)>& consumer) const;

	int32_t size() const;

	/** @return the map template, nullptr (Java null) if there is none */
	const model::templates::world::WorldMapTemplate* getTemplate(int32_t worldId) const;

	int32_t getWorldIdByCName(std::string_view name) const;
};

} // namespace aion::gameserver::dataholders
