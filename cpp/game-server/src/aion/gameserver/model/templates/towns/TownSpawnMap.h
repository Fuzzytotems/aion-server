#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/detail/FlatMap.h"
#include "aion/gameserver/model/templates/towns/TownSpawnMap.xml.h"

namespace aion::gameserver::model::templates::towns {

/**
 * Java com.aionemu.gameserver.model.templates.towns.TownSpawnMap.
 * <p>
 * C++: the @XmlTransient `townSpawnsData` is a C++-only map (detail::FlatMap: bound templates need noexcept moves) of template pointers into the
 * bound list, which the hook fills and keeps (Java nulls
 * the list); the hook also records the values in Java's HashMap<Integer> iteration order for getTownSpawns().
 *
 * @author ViAl
 */
class TownSpawnMap : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/towns/TownSpawnMap.xml.inc"
private:
	/** Java @XmlTransient Map<Integer, TownSpawn> townSpawnsData */
	::aion::gameserver::model::templates::detail::FlatMap<int32_t, const TownSpawn*> townSpawnsData;
	/** C++ only: townSpawnsData.values() in Java's HashMap iteration order */
	std::vector<const TownSpawn*> townSpawnsDataValues;

public:
	/** @return the entry, nullptr (Java null) if there is none */
	const TownSpawn* getTownSpawn(int32_t townId) const;

	/** Java townSpawnsData.values() */
	const std::vector<const TownSpawn*>& getTownSpawns() const { return townSpawnsDataValues; }
};

} // namespace aion::gameserver::model::templates::towns
