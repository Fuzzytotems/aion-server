#include "aion/gameserver/model/flyring/FlyRing.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/controllers/FlyRingController.h"
#include "aion/gameserver/model/geometry/Plane3D.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::model::flyring {

namespace {

template <class T>
const T* nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(what);
	return value;
}

template <class Point>
geoEngine::math::Vector3f vectorOf(const Point* point) {
	return geoEngine::math::Vector3f(point->getX(), point->getY(), point->getZ());
}

/**
 * Java super(...) argument: World.getInstance().createPosition(template.getMap(), center x, y, z, (byte) 0, instanceId), with Java's
 * NullPointerExceptions for a null template or center (evaluated in sequence here; C++ leaves the order of call arguments unspecified).
 */
runtime::Ref<world::WorldPosition> positionOf(const templates::flyring::FlyRingTemplate* template_, int32_t instanceId) {
	const auto* center = nonNull(nonNull(template_, "template")->getCenter(), "template.getCenter()");
	return world::World::getInstance().createPosition(template_->getMap(), center->getX(), center->getY(), center->getZ(), int8_t{0}, instanceId);
}

} // namespace

FlyRing::FlyRing(CreateKey key, const templates::flyring::FlyRingTemplate* templateValue, int32_t instanceId)
	: VisibleObject(key, utils::idfactory::IDFactory::getInstance().nextId(), std::make_unique<controllers::FlyRingController>(), nullptr, nullptr,
		  positionOf(templateValue, instanceId), true),
	  template_(templateValue),
	  plane(geometry::Plane3D::create(vectorOf(templateValue->getCenter()), vectorOf(nonNull(templateValue->getP1(), "template.getP1()")),
		  vectorOf(nonNull(templateValue->getP2(), "template.getP2()")))) {
	// Java stores template and plane after setOwner; the const members are initialized first here (setOwner only binds the part)
	getController().setOwner(*this);
	setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*this));
}

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
