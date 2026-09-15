#include "aion/gameserver/model/templates/quest/QuestRepeatCycleInfo.h"

#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::model::templates::quest {

std::string getL10n(QuestRepeatCycle cycle) {
	return utils::ChatUtil::l10n(getL10nId(cycle));
}

} // namespace aion::gameserver::model::templates::quest
