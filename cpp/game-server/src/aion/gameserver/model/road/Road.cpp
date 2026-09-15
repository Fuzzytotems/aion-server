#include "aion/gameserver/model/road/Road.h"

#include <optional>
#include <string>

#include "aion/gameserver/model/geometry/Plane3D.h"
#include "aion/gameserver/model/templates/road/RoadTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::road {

// Road(CreateKey, const RoadTemplate*, std::optional<int32_t>) is defined once controllers/RoadController.h exists (P4-11b). Java:
//   super(IDFactory.nextId(), new RoadController(), null, null, World.getInstance().createPosition(template.getMap(), center x, y, z, (byte) 0,
//   instanceId), true); ((RoadController) getController()).setOwner(this); this.template = template;
//   this.plane = new Plane3D(center, p1, p2); setKnownlist(new PlayerAwareKnownList(this))

Road::~Road() = default;

bool Road::isCrossed(const geoEngine::math::Vector3f& oldPosition, const geoEngine::math::Vector3f& newPosition) {
	std::optional<geoEngine::math::Vector3f> intersection = plane->intersection(oldPosition, newPosition);
	return intersection && utils::PositionUtil::isInRange(*this, intersection->x, intersection->y, intersection->z, template_->getRadius());
}

std::string Road::getName() {
	return template_->getName();
}

void Road::spawn() {
	world::World& w = world::World::getInstance();
	w.storeObject(*this);
	w.spawn(*this);
}

} // namespace aion::gameserver::model::road
