#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/quest/QuestDrop.h"

namespace aion::gameserver::model::templates::quest {

/**
 * A quest drop a quest handler registers at run time (if not already in xml).
 * <p>
 * C++: QuestDrop declares HandlerSideDrop a friend, since Java reads the protected fields of other drops (same package).
 * <p>
 * C++: fieldmap infers K5, but QuestEngine.addHandlerSideQuestDrop stores the object in QuestService.questDrop, which holds template pointers
 * (`const QuestDrop*`): the QuestEngine port allocates it once per registration at startup and never frees it, like a template.
 *
 * @author vlog, Rolandas
 */
class HandlerSideDrop : public QuestDrop {
private:
	int32_t neededAmount = 0;

public:
	/** @throws NullPointerException (Java) if the quest has no template */
	HandlerSideDrop(int32_t questId, int32_t npcId, int32_t itemId, int32_t amount, int32_t chance);

	HandlerSideDrop(int32_t questId, int32_t npcId, int32_t itemId, int32_t amount, int32_t chance, int32_t step);

	int32_t getNeededAmount() const { return neededAmount; }
};

} // namespace aion::gameserver::model::templates::quest
