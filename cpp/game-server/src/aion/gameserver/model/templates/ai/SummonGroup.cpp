#include "aion/gameserver/model/templates/ai/SummonGroup.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::model::templates::ai {

void SummonGroup::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Deviation: Java throws IllegalArgumentException, which fails the unmarshalling; LoadContext::fail throws StaticDataException with the
	// same message and the XML location (docs/deviations/P4-07b.md)
	if (minCount <= 0)
		ctx.fail("minCount (" + std::to_string(minCount) + ") for npc group " + std::to_string(npcId) + " must be greater than zero");
	if (maxCount == 0)
		maxCount = minCount;
	else if (maxCount < minCount)
		ctx.fail("maxCount (" + std::to_string(maxCount) + ") for npc group " + std::to_string(npcId) + " must be greater than minCount (" +
		         std::to_string(minCount) + ")");
}

} // namespace aion::gameserver::model::templates::ai
