#include "aion/gameserver/model/stats/listeners/TitleChangeListener.h"

#include <memory>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/templates/TitleTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::stats::listeners {

void TitleChangeListener::onBonusTitleChange(container::CreatureGameStats& cgs, int32_t titleId, bool isSet) {
	const templates::TitleTemplate* tt = dataholders::DataManager::TITLE_DATA->getTitleTemplate(titleId);
	if (tt == nullptr) {
		return;
	}
	// templates are stat owners by identity; the interface has no mutating member (StatOwner.h)
	auto& titleOwner = const_cast<templates::TitleTemplate&>(*tt);
	if (!isSet) {
		cgs.endEffect(titleOwner);
	} else {
		const std::vector<std::unique_ptr<calc::functions::StatFunction>>* modifiers = tt->getModifiers();
		if (modifiers == nullptr) // Java: addEffect(tt, null) iterates a null list
			throw runtime::NullPointerException("title modifiers are null");
		std::vector<runtime::Ptr<calc::functions::IStatFunction>> functions;
		for (const std::unique_ptr<calc::functions::StatFunction>& modifier : *modifiers)
			functions.emplace_back(calc::functions::StatFunction::ofTemplate(modifier.get()));
		cgs.addEffect(runtime::Ptr<calc::StatOwner>(titleOwner), functions);
	}
}

} // namespace aion::gameserver::model::stats::listeners
