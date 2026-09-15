#include "aion/gameserver/model/templates/item/RandomItem.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::model::templates::item {

namespace {
/** Java string concatenation of the nullable enum `type` */
std::string typeString(const std::optional<RandomType>& type) {
	return type ? std::string(xml::enumName(*type)) : std::string("null");
}
} // namespace

void RandomItem::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Deviation: Java throws IllegalArgumentException, which fails the unmarshalling; LoadContext::fail throws StaticDataException with the
	// same message and the XML location (docs/deviations/P4-07a.md)
	if (minCount <= 0)
		ctx.fail("Decomposable random reward item of type " + typeString(type) + " min_count (" + std::to_string(minCount) + ") must be greater than 0");
	if (maxCount == 0)
		maxCount = minCount;
	else if (maxCount < minCount)
		ctx.fail("Decomposable random reward item of type " + typeString(type) + " max_count (" + std::to_string(maxCount) +
			") must be unset or greater than min_count (" + std::to_string(minCount) + ")");
}

} // namespace aion::gameserver::model::templates::item
