#include "aion/gameserver/model/road/Road.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/controllers/RoadController.h"
#include "aion/gameserver/model/geometry/Plane3D.h"
#include "aion/gameserver/model/templates/road/RoadTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::model::road {

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
runtime::Ref<world::WorldPosition> positionOf(const templates::road::RoadTemplate* template_, std::optional<int32_t> instanceId) {
	const auto* center = nonNull(nonNull(template_, "template")->getCenter(), "template.getCenter()");
	if (!instanceId)
		throw runtime::NullPointerException("instanceId"); // Java: the Integer is unboxed to int
	return world::World::getInstance().createPosition(template_->getMap(), center->getX(), center->getY(), center->getZ(), int8_t{0}, *instanceId);
}

} // namespace

Road::Road(CreateKey key, const templates::road::RoadTemplate* templateValue, std::optional<int32_t> instanceId)
	: VisibleObject(key, utils::idfactory::IDFactory::getInstance().nextId(), std::make_unique<controllers::RoadController>(), nullptr, nullptr,
		  positionOf(templateValue, instanceId), true),
	  template_(templateValue),
	  plane(geometry::Plane3D::create(vectorOf(templateValue->getCenter()), vectorOf(nonNull(templateValue->getP1(), "template.getP1()")),
		  vectorOf(nonNull(templateValue->getP2(), "template.getP2()")))) {
	// Java stores template and plane after setOwner; the const members are initialized first here (setOwner only binds the part)
	getController().setOwner(*this);
	setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*this));
}

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
