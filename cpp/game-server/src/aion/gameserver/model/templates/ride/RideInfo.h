#pragma once

#include "aion/gameserver/model/templates/ride/RideInfo.xml.h"

namespace aion::gameserver::model::templates::ride {

/** Java com.aionemu.gameserver.model.templates.ride.RideInfo. @author Rolandas */
class RideInfo : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/ride/RideInfo.xml.inc"
public:
	bool canSprint() const { return sprintSpeed != 0.0f; }
};

} // namespace aion::gameserver::model::templates::ride
