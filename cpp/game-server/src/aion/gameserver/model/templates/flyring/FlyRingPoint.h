#pragma once

#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/flyring/FlyRingPoint.xml.h"

namespace aion::gameserver::model::templates::flyring {

/** Java com.aionemu.gameserver.model.templates.flyring.FlyRingPoint. @author M@xx */
class FlyRingPoint : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/flyring/FlyRingPoint.xml.inc"
public:
	FlyRingPoint() = default;

	explicit FlyRingPoint(geometry::Point3D& p);
};

} // namespace aion::gameserver::model::templates::flyring
