#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.xml.h"

namespace aion::gameserver::model::templates::flyring {

/**
 * Java com.aionemu.gameserver.model.templates.flyring.FlyRingTemplate.
 * <p>
 * C++: instance handlers create templates at run time (`new FlyRing(new FlyRingTemplate(...))`) and FlyRing keeps a template pointer, so
 * those callers allocate the template once and never free it, like static data.
 *
 * @author M@xx
 */
class FlyRingTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.xml.inc"
public:
	FlyRingTemplate() = default;

	FlyRingTemplate(std::string_view name, int32_t mapId, geometry::Point3D& center, geometry::Point3D& p1, geometry::Point3D& p2, int32_t radius);
};

} // namespace aion::gameserver::model::templates::flyring
