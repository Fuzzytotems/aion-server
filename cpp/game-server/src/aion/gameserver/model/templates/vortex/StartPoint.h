#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/fwd.h"

#include "aion/gameserver/model/templates/vortex/StartPoint.xml.h"

namespace aion::gameserver::model::templates::vortex {

/** Java com.aionemu.gameserver.model.templates.vortex.StartPoint. @author Source */
class StartPoint : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/vortex/StartPoint.xml.inc"
public:
	/** @return a new position on the map with the coordinates of this point (Java: new WorldPosition(map) and setXYZH) */
	runtime::Ref<::aion::gameserver::world::WorldPosition> getStartPoint() const;
};

} // namespace aion::gameserver::model::templates::vortex
