#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/model/templates/housing/Building.xml.h"

#include "aion/gameserver/model/templates/detail/FlatMap.h"
#include "aion/gameserver/model/templates/housing/PartType.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Java com.aionemu.gameserver.model.templates.housing.Building.
 * <p>
 * C++: the @XmlTransient `partsByType` (an EnumMap) is a C++-only map in ordinal order, filled by the hook (a detail::FlatMap: bound templates
 * need noexcept moves). An absent `parts_match` attribute is the empty string, Java's null.
 *
 * @author Rolandas
 */
class Building : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/housing/Building.xml.inc"
private:
	/** Java @XmlTransient Map<PartType, Integer> partsByType (null without parts) */
	std::optional<::aion::gameserver::model::templates::detail::FlatMap<PartType, int32_t>> partsByType;

public:
	// All DataManager calls are just to ensure integrity if called from housing land templates. Because buildings in land templates have only id
	// and isDefault set. Buildings template has full info though, except isDefault value for the land.
	const std::string& getPartsMatchTag() const;

	std::optional<HouseType> getSize() const;

	std::optional<BuildingType> getType() const;

	/** @return the default decoration of the part type, null (nullopt) if there is none */
	std::optional<int32_t> getDefaultDecorId(PartType partType) const;

	/** @return the default part ids in part type order (Java: new ArrayList of the EnumMap values) */
	std::vector<int32_t> getDefaultPartIds() const;

private:
	/** @throws NullPointerException (Java) if neither this building nor the holder's building has parts */
	const ::aion::gameserver::model::templates::detail::FlatMap<PartType, int32_t>& getPartsByType() const;
};

} // namespace aion::gameserver::model::templates::housing
