#pragma once

#include <vector>

#include "aion/gameserver/model/templates/zone/Points.xml.h"

namespace aion::gameserver::model::templates::zone {

/** Java com.aionemu.gameserver.model.templates.zone.Points. @author ATracer */
class Points : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/zone/Points.xml.inc"
public:
	Points() = default;

	Points(float bottom, float top);

	/** Java creates the list on first use; the C++ vector always exists */
	const std::vector<Point2D>& getPoint() const { return point; }

	/** C++ only: the live list for templates built at run time (WorldZoneTemplate) */
	std::vector<Point2D>& getPoint() { return point; }
};

} // namespace aion::gameserver::model::templates::zone
