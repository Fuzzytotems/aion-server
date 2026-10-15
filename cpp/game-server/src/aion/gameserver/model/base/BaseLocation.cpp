#include "aion/gameserver/model/base/BaseLocation.h"

#include <string>

#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/base/BaseType.h"
#include "aion/gameserver/model/templates/base/BaseTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::base {

namespace {
/** Java: template.getType() - every base_location of base_locations.xml has a type; a missing one is Java's null, refused here (P5-12b.md) */
BaseType typeOf(const templates::base::BaseTemplate* value) {
	if (!value->getType())
		throw runtime::NullPointerException("BaseTemplate.type of base " + std::to_string(value->getId()));
	return *value->getType();
}
} // namespace

// Java BaseLocation.java:18-22
BaseLocation::BaseLocation(const templates::base::BaseTemplate* value) : template_(value) {
	this->type.set(typeOf(value));
	this->occupier.set(value->getDefaultOccupier());
}

runtime::Ref<BaseLocation> BaseLocation::create(const templates::base::BaseTemplate* value) {
	return runtime::makeRef<BaseLocation>(value);
}

// Java BaseLocation.java:24-26
int32_t BaseLocation::getId() {
	return template_.get()->getId();
}

// Java BaseLocation.java:28-30
int32_t BaseLocation::getWorldId() {
	return template_.get()->getWorldId();
}

const templates::base::BaseTemplate* BaseLocation::getTemplate() const {
	return template_.get();
}

BaseLocation::~BaseLocation() = default;

} // namespace aion::gameserver::model::base
