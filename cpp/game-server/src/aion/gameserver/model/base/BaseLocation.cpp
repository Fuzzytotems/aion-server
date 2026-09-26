#include "aion/gameserver/model/base/BaseLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/base/BaseTemplate.h"

namespace aion::gameserver::model::base {

BaseLocation::BaseLocation(const templates::base::BaseTemplate* value)
	: template_(value) {
	// Java: this.type = template.getType(); this.occupier = template.getDefaultOccupier()
	AION_UNPORTED();
}

runtime::Ref<BaseLocation> BaseLocation::create(const templates::base::BaseTemplate* value) {
	return runtime::makeRef<BaseLocation>(value);
}

int32_t BaseLocation::getId() {
	AION_UNPORTED();
}

int32_t BaseLocation::getWorldId() {
	AION_UNPORTED();
}

const templates::base::BaseTemplate* BaseLocation::getTemplate() const {
	return template_.get();
}

BaseLocation::~BaseLocation() = default;

} // namespace aion::gameserver::model::base
