#include "aion/gameserver/model/base/PanesterraBaseLocation.h"

#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/base/BaseType.h"
#include "aion/gameserver/model/templates/base/BaseTemplate.h"

namespace aion::gameserver::model::base {

// Java PanesterraBaseLocation.java:9-13
PanesterraBaseLocation::PanesterraBaseLocation(const templates::base::BaseTemplate* value) : BaseLocation(value) {
	if (value->getType() == BaseType::PANESTERRA_FACTION_CAMP)
		occupier.set(BaseOccupier::PEACE);
}

runtime::Ref<PanesterraBaseLocation> PanesterraBaseLocation::create(const templates::base::BaseTemplate* value) {
	return runtime::makeRef<PanesterraBaseLocation>(value);
}

PanesterraBaseLocation::~PanesterraBaseLocation() = default;

} // namespace aion::gameserver::model::base
