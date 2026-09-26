#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"

#include <memory>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::walker {

namespace {
/**
 * Java String.split(","): without a comma the whole text is the only part; otherwise the parts between commas with every trailing empty part
 * removed, so a text of commas only gives no parts
 */
std::vector<std::string_view> splitComma(std::string_view text) {
	if (text.find(',') == std::string_view::npos)
		return {text};
	std::vector<std::string_view> parts;
	size_t begin = 0;
	while (true) {
		size_t end = text.find(',', begin);
		parts.push_back(text.substr(begin, end == std::string_view::npos ? std::string_view::npos : end - begin));
		if (end == std::string_view::npos)
			break;
		begin = end + 1;
	}
	while (!parts.empty() && parts.back().empty())
		parts.pop_back();
	return parts;
}
} // namespace

WalkerTemplate::WalkerTemplate(std::string_view routeIdValue) {
	this->routeId = std::string(routeIdValue);
}

void WalkerTemplate::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	if (routeStepList.empty()) // Deviation: Java throws IndexOutOfBoundsException on get(-1); routestep is required (docs/deviations/P4-07b.md)
		ctx.fail("Walker template " + routeId + " has no route steps");
	if (loopType == LoopType::WALK_BACK) { // add steps in backward order, so npcs turn and walk the same way back
		for (int32_t i = static_cast<int32_t>(routeStepList.size()) - 2; i > 0; i--) { // skip first and last step
			const RouteStep& step = *routeStepList[static_cast<size_t>(i)];
			routeStepList.push_back(std::make_unique<RouteStep>(step.getX(), step.getY(), step.getZ(), step.getRestTime()));
		}
	}
	for (size_t i = 0; i + 1 < routeStepList.size(); i++) {
		RouteStep& step = *routeStepList[i];
		step.setStepIndex(static_cast<int32_t>(i));
	}
	RouteStep& lastStep = *routeStepList.back();
	lastStep.setStepIndex(static_cast<int32_t>(routeStepList.size() - 1));
	lastStep.setIsLastStep(true);
	if (pool == 2) {
		formation = spawnengine::WalkerGroupType::SQUARE;
		rows = std::vector<int32_t>{2};
	} else if (formation == spawnengine::WalkerGroupType::SQUARE) {
		// Deviation: Java rowValues != null; the binder stores an absent attribute as the empty string, so a present but empty `rows` (Java:
		// NumberFormatException for "") also counts as absent (docs/deviations/P4-07b.md)
		if (!rowValues.empty()) {
			std::vector<int32_t> values;
			for (std::string_view value : splitComma(rowValues)) {
				try {
					values.push_back(commons::utils::parseInt(value));
				} catch (const commons::utils::NumberFormatException& e) {
					// Deviation: Java's NumberFormatException fails the unmarshalling; LoadContext::fail reports it with the location
					ctx.fail(std::string("Walker template ") + routeId + " rows: " + e.what());
				}
			}
			rows = std::move(values);
		} else {
			formation = spawnengine::WalkerGroupType::POINT;
		}
	}
	rowValues.clear(); // Java: rowValues = null
}

const RouteStep* WalkerTemplate::getRouteStep(int32_t stepIndex) const {
	if (stepIndex < 0 || static_cast<size_t>(stepIndex) >= routeStepList.size())
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(stepIndex) + " out of bounds for length " +
		                                         std::to_string(routeStepList.size()));
	return routeStepList[static_cast<size_t>(stepIndex)].get();
}

std::optional<std::string> WalkerTemplate::getVersionId() const {
	return dataholders::DataManager::WALKER_VERSIONS_DATA->getRouteVersionId(routeId);
}

} // namespace aion::gameserver::model::templates::walker
