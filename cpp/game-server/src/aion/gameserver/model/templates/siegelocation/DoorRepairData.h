#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/detail/FlatMap.h"
#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/**
 * Java com.aionemu.gameserver.model.templates.siegelocation.DoorRepairData.
 * <p>
 * C++: the @XmlTransient `doorRepairStones` is a C++-only map of template pointers into the bound stones, which the hook fills and keeps (Java
 * nulls the list; a detail::FlatMap: bound templates need noexcept moves). getRepairStones() returns them by ascending static id (Java: HashMap
 * order; nothing calls it).
 */
class DoorRepairData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.xml.inc"
private:
	/** Java @XmlTransient Map<Integer, DoorRepairStone> doorRepairStones */
	::aion::gameserver::model::templates::detail::FlatMap<int32_t, const DoorRepairStone*> doorRepairStones;

public:
	/** Java: cd * 1000 (int arithmetic) */
	int32_t getCd() const { return static_cast<int32_t>(static_cast<uint32_t>(cd) * 1000u); }

	/** @return the stone, nullptr (Java null) for an unknown static id */
	const DoorRepairStone* getRepairStone(int32_t stoneStaticId) const;

	/** Java doorRepairStones.values() */
	std::vector<const DoorRepairStone*> getRepairStones() const;
};

} // namespace aion::gameserver::model::templates::siegelocation
