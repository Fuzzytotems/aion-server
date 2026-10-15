#include "aion/gameserver/model/base/StainedBaseLocation.h"

#include <string>

#include "aion/gameserver/model/templates/base/BaseTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::base {

namespace {
/** Java: template.getColor() - every STAINED base_location has a colour; a missing one (Java's null) is refused here (P5-12b.md) */
BaseColorType colorOf(const templates::base::BaseTemplate* value) {
	if (!value->getColor())
		throw runtime::NullPointerException("BaseTemplate.color of base " + std::to_string(value->getId()));
	return *value->getColor();
}
} // namespace

} // namespace aion::gameserver::model::base

namespace aion::gameserver::model::base {

// Java StainedBaseLocation.java:13-16
StainedBaseLocation::StainedBaseLocation(const templates::base::BaseTemplate* value) : BaseLocation(value), color(colorOf(value)) {
}

runtime::Ref<StainedBaseLocation> StainedBaseLocation::create(const templates::base::BaseTemplate* value) {
	return runtime::makeRef<StainedBaseLocation>(value);
}

StainedBaseLocation::~StainedBaseLocation() = default;

} // namespace aion::gameserver::model::base
