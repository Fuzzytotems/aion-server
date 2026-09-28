#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/fwd.h"

#include "aion/gameserver/model/templates/vortex/ResurrectionPoint.xml.h"

namespace aion::gameserver::model::templates::vortex {

/** Java com.aionemu.gameserver.model.templates.vortex.ResurrectionPoint. @author Source */
class ResurrectionPoint : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/vortex/ResurrectionPoint.xml.inc"
public:
	/** @return a new position on the map with the coordinates of this point (Java: new WorldPosition(map) and setXYZH) */
	runtime::Ref<::aion::gameserver::world::WorldPosition> getResurrectionPoint() const;
};

} // namespace aion::gameserver::model::templates::vortex
