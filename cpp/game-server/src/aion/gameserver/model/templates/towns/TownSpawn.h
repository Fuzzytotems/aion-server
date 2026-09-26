#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/detail/FlatMap.h"
#include "aion/gameserver/model/templates/towns/TownSpawn.xml.h"

namespace aion::gameserver::model::templates::towns {

/**
 * Java com.aionemu.gameserver.model.templates.towns.TownSpawn.
 * <p>
 * C++: the @XmlTransient `townLevelsData` is a C++-only map (detail::FlatMap: bound templates need noexcept moves) of template pointers into the
 * bound list, which the hook fills and keeps (Java nulls
 * the list); the hook also records the values in Java's HashMap<Integer> iteration order for getTownLevels().
 *
 * @author ViAl
 */
class TownSpawn : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/towns/TownSpawn.xml.inc"
private:
	/** Java @XmlTransient Map<Integer, TownLevel> townLevelsData */
	::aion::gameserver::model::templates::detail::FlatMap<int32_t, const TownLevel*> townLevelsData;
	/** C++ only: townLevelsData.values() in Java's HashMap iteration order */
	std::vector<const TownLevel*> townLevelsDataValues;

public:
	/** @return the entry, nullptr (Java null) if there is none */
	const TownLevel* getSpawnsForLevel(int32_t level) const;

	/** Java townLevelsData.values() */
	const std::vector<const TownLevel*>& getTownLevels() const { return townLevelsDataValues; }
};

} // namespace aion::gameserver::model::templates::towns
