#pragma once

// TEST SHELL for the generated-code slice test (GeneratedSliceTest.cpp), created with `xmlgen.py scaffold dataholders.WorldMapsData`. It stands
// in for the hand-written class of the static data port (P4-09) and ports only what the slice test checks: the index built by the hook.

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/WorldMapsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.WorldMapsData (test shell). @author Luno */
class WorldMapsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WorldMapsData.xml.inc"
public:
	/** Java: size() */
	size_t size() const { return mapsById.size(); }
	/** Java: getTemplate(int) (nullptr for unknown ids) */
	const model::templates::world::WorldMapTemplate* getTemplate(int32_t worldId) const {
		auto it = mapsById.find(worldId);
		return it == mapsById.end() ? nullptr : it->second;
	}
	const std::vector<model::templates::world::WorldMapTemplate>& getWorldMaps() const { return worldMaps; }

private:
	std::unordered_map<int32_t, const model::templates::world::WorldMapTemplate*> mapsById;
};

/** Java: mapsById.put for every map (the storage stays, design §2.6) */
inline void WorldMapsData::afterUnmarshal(xml::LoadContext&, const xml::XmlParent&) {
	for (const model::templates::world::WorldMapTemplate& map : worldMaps)
		mapsById[map.getMapId()] = &map;
}

} // namespace aion::gameserver::dataholders
