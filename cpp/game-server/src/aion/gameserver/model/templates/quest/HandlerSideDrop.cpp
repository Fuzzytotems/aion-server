#include "aion/gameserver/model/templates/quest/HandlerSideDrop.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::quest {

namespace {
/** Java auto-unboxing of a null Integer field in `drop.npcId == npcId` */
int32_t unbox(const std::optional<int32_t>& value, const char* field) {
	if (!value)
		throw runtime::NullPointerException(std::string("QuestDrop.") + field + " is null");
	return *value;
}
} // namespace

HandlerSideDrop::HandlerSideDrop(int32_t questIdValue, int32_t npcIdValue, int32_t itemIdValue, int32_t amount, int32_t chanceValue) {
	this->questId = questIdValue;
	this->npcId = npcIdValue;
	this->itemId = itemIdValue;
	this->chance = chanceValue;
	const QuestTemplate* quest = dataholders::DataManager::QUEST_DATA->getQuestById(questIdValue);
	if (quest == nullptr)
		throw runtime::NullPointerException("No quest template for handler side drop of quest " + std::to_string(questIdValue));
	for (const QuestDrop& drop : quest->getQuestDrop()) {
		if (unbox(drop.npcId, "npcId") == npcIdValue && unbox(drop.itemId, "itemId") == itemIdValue) {
			this->dropEachMember = drop.dropEachMember;
			break;
		}
	}
	this->neededAmount = amount;
}

HandlerSideDrop::HandlerSideDrop(int32_t questIdValue, int32_t npcIdValue, int32_t itemIdValue, int32_t amount, int32_t chanceValue, int32_t step)
    : HandlerSideDrop(questIdValue, npcIdValue, itemIdValue, amount, chanceValue) {
	this->collecting_step = step;
}

} // namespace aion::gameserver::model::templates::quest
