#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.h"

#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/AbsoluteStatsData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/templates/stats/ModifiersTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::skillengine::effect {

namespace functions = gameserver::model::stats::calc::functions;

std::vector<runtime::Ref<functions::IStatFunction>> AbstractAbsoluteStatEffect::getModifiers(model::Effect& /*effect*/) const {
	std::vector<runtime::Ref<functions::IStatFunction>> modifiers;
	const gameserver::model::templates::stats::ModifiersTemplate* modifiersSet = getModifiersSet();
	if (modifiersSet == nullptr) // Java: getModifiersSet().getModifiers() on an unknown statsetid is a NullPointerException
		throw runtime::NullPointerException("no absolute stats set " + std::to_string(statSetId));
	// Java adds the template's own functions (static data: StatFunction::ofTemplate holds them without a count)
	for (const std::unique_ptr<functions::StatFunction>& modifier : modifiersSet->getModifiers())
		modifiers.push_back(runtime::Ref<functions::IStatFunction>(*functions::StatFunction::ofTemplate(modifier.get())));
	return modifiers;
}

const gameserver::model::templates::stats::ModifiersTemplate* AbstractAbsoluteStatEffect::getModifiersSet() const {
	return dataholders::DataManager::ABSOLUTE_STATS_DATA->getTemplate(statSetId);
}

} // namespace aion::gameserver::skillengine::effect
