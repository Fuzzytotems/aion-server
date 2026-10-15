#include "aion/gameserver/model/base/SiegeBaseLocation.h"


namespace aion::gameserver::model::base {

// Java SiegeBaseLocation.java:10-12
SiegeBaseLocation::SiegeBaseLocation(const templates::base::BaseTemplate* value) : BaseLocation(value) {
}

runtime::Ref<SiegeBaseLocation> SiegeBaseLocation::create(const templates::base::BaseTemplate* value) {
	return runtime::makeRef<SiegeBaseLocation>(value);
}

SiegeBaseLocation::~SiegeBaseLocation() = default;

} // namespace aion::gameserver::model::base
