#pragma once

#include <cstdint>
#include <memory>

#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/fwd.h"

namespace aion::gameserver::model::templates::zone {

/**
 * The zone template of a whole map (ZoneService): a square of the map size, named by the map id. A value class (fieldmap K5) that ZoneService
 * builds at run time.
 * <p>
 * C++ interim (upstream sync foundation, docs/deviations/P4-07a.md): upstream 2b097879f removed this class and ZoneClassName.DUMMY, its zone
 * type. Until wave 4 ports the zone rework the class stays with the zone type SUB (the Java default); ZoneTemplate::isWholeMapZone takes the place of the
 * DUMMY checks, and MapRegion sorts the whole map zone first as DUMMY, the lowest ordinal, did.
 *
 * @author Rolandas
 */
class WorldZoneTemplate : public ZoneTemplate {
public:
	/** Java: reads the flags of the map from DataManager.WORLD_MAPS_DATA */
	WorldZoneTemplate(int32_t size, int32_t mapId);

	/** C++ only: the Java constructor with the flags of the map passed in (WorldMapTemplate.getFlags()) */
	WorldZoneTemplate(int32_t size, int32_t mapId, int32_t mapFlags);

	/**
	 * C++ only: the points the constructor builds - bottom -1 and top maxZ + 1 with maxZ = Math.round((float) size / regionSize) * regionSize
	 * (int multiplication, then widened to float), and the corners (-1, -1), (-1, size + 1), (size + 1, size + 1), (size + 1, -1).
	 */
	static std::unique_ptr<Points> createMapPoints(int32_t size, int32_t regionSize);

};

} // namespace aion::gameserver::model::templates::zone
