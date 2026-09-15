#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/detail/FlatMap.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorWorld.xml.h"

namespace aion::gameserver::model::templates::staticdoor {

/**
 * Java com.aionemu.gameserver.model.templates.staticdoor.StaticDoorWorld.
 * <p>
 * C++: the @XmlTransient `templatesByStaticId` is a C++-only map of template pointers into the bound doors, which the hook fills and keeps (Java
 * nulls the list; a detail::FlatMap: bound templates need noexcept moves); the hook also records the doors in Java's HashMap<Integer> iteration
 * order for getStaticDoors().
 *
 * @author xTz
 */
class StaticDoorWorld : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/staticdoor/StaticDoorWorld.xml.inc"
private:
	/** Java @XmlTransient Map<Integer, StaticDoorTemplate> templatesByStaticId */
	::aion::gameserver::model::templates::detail::FlatMap<int32_t, const StaticDoorTemplate*> templatesByStaticId;
	/** C++ only: templatesByStaticId.values() in Java's HashMap iteration order */
	std::vector<const StaticDoorTemplate*> staticDoorsInHashOrder;

public:
	/** Java templatesByStaticId.values() */
	const std::vector<const StaticDoorTemplate*>& getStaticDoors() const { return staticDoorsInHashOrder; }

	/** @return the door, nullptr (Java null) for an unknown static id */
	const StaticDoorTemplate* getStaticDoor(int32_t staticId) const;
};

} // namespace aion::gameserver::model::templates::staticdoor
