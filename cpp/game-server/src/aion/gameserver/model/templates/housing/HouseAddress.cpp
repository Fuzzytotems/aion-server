#include "aion/gameserver/model/templates/housing/HouseAddress.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/XmlParent.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"

namespace aion::gameserver::model::templates::housing {

void HouseAddress::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& parent) {
	// Java: this.land = Objects.requireNonNull((HousingLand) parent), which throws (NullPointerException or ClassCastException) outside a land.
	// Deviation: LoadContext::fail throws StaticDataException with the location instead (docs/deviations/P4-07b.md)
	const HousingLand* housingLand = parent.as<HousingLand>();
	if (housingLand == nullptr)
		ctx.fail("HouseAddress " + std::to_string(id) + " is not bound inside a HousingLand");
	this->land = housingLand;
}

} // namespace aion::gameserver::model::templates::housing
