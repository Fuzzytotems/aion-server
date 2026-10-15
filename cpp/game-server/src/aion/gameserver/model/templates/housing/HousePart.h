#pragma once

#include "aion/gameserver/model/templates/housing/HousePart.xml.h"

#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.HousePart. @author Rolandas */
class HousePart : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/housing/HousePart.xml.inc"
public:
	/** @throws NullPointerException (Java) without building_tags (a required attribute, always bound) */
	bool isForBuilding(const Building& building) const;
};

} // namespace aion::gameserver::model::templates::housing
