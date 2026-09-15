#include "aion/gameserver/model/flyring/FlyRing.h"

#include <optional>
#include <string>

#include "aion/gameserver/model/geometry/Plane3D.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::flyring {

// FlyRing(CreateKey, const FlyRingTemplate*, int32_t) is defined once controllers/FlyRingController.h exists (P4-11b). Java:
//   super(IDFactory.nextId(), new FlyRingController(), null, null, World.getInstance().createPosition(template.getMap(), center x, y, z, (byte) 0,
//   instanceId), true); ((FlyRingController) getController()).setOwner(this); this.template = template;
//   this.plane = new Plane3D(center, p1, p2); setKnownlist(new PlayerAwareKnownList(this))

FlyRing::~FlyRing() = default;

bool FlyRing::isCrossed(const geoEngine::math::Vector3f& oldPosition, const geoEngine::math::Vector3f& newPosition) {
	std::optional<geoEngine::math::Vector3f> intersection = plane->intersection(oldPosition, newPosition);
	return intersection && utils::PositionUtil::isInRange(*this, intersection->x, intersection->y, intersection->z, template_->getRadius());
}

std::string FlyRing::getName() {
	return template_->getName();
}

void FlyRing::spawn() {
	world::World::getInstance().spawn(*this);
}

} // namespace aion::gameserver::model::flyring
