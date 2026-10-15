#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/HousingLand.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousingLand. @author Rolandas */
class HousingLand : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/housing/HousingLand.xml.inc"
public:
	/**
	 * @return the default building, else the first one
	 * @throws IndexOutOfBoundsException (Java) without buildings
	 */
	const Building* getDefaultBuilding() const;

	int32_t hashCode() const { return id; }
};

} // namespace aion::gameserver::model::templates::housing
