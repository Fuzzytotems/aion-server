#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.xml.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Java com.aionemu.gameserver.model.templates.housing.PlaceableHouseObject.
 * <p>
 * C++: Java's abstract `byte getTypeId()` would be a new virtual function of a frozen shell (a layout change); getTypeId() is a non-virtual
 * function of this class that selects the value of the concrete class by javaClassName() (the generated virtual), and each subclass declares the
 * same non-virtual getter with its own value, so both static and dynamic calls give Java's result.
 *
 * @author Rolandas
 */
class PlaceableHouseObject : public ::aion::gameserver::model::templates::housing::AbstractHouseObject {
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.xml.inc"
public:
	/** @return 0 without use_days */
	int32_t getUseDays() const { return useDays.value_or(0); }

	/** @return LimitType::NONE without a limit attribute */
	LimitType getPlacementLimit() const { return limit.value_or(LimitType::NONE); }

	/** Java abstract getTypeId(): the value of the concrete class */
	int8_t getTypeId() const;
};

} // namespace aion::gameserver::model::templates::housing
