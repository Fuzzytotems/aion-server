#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler: the base of the handlers the XML quests
 * (quest_script_data) are made of. XMLQuest::register_ of each kind constructs one and hands it to QuestEngine::addQuestHandler.
 * <p>
 * C++ differences of the templates:
 * - The two static helpers are C++ only; they spell Java expressions the template constructors and hooks repeat.
 * - The templates' npc-id sets (Java `new HashSet<>()`) are runtime::HashSet filled in the order of the bound lists, and register_ walks
 *   them in that order, not in Java's HashSet order. Every registration of such a loop lands in the lists of its own npc, so the per-npc
 *   lists come out the same; only the order of QuestEngine::registerCanAct's warnings for npc ids without a template can differ
 *   (docs/deviations/P5-06c.md).
 */
class AbstractTemplateQuestHandler : public AbstractQuestHandler {
protected:
	explicit AbstractTemplateQuestHandler(int32_t questId);

	/** C++ only: Java `a.equals(b)` of two sets (AbstractSet.equals: the same size, and a contains every element of b) */
	static bool equalSets(const runtime::HashSet<int32_t>& a, const runtime::HashSet<int32_t>& b);

	/** C++ only: Java `DataManager.QUEST_DATA.getQuestById(questId)` dereferenced (NullPointerException for an unknown quest) */
	static const gameserver::model::templates::QuestTemplate& questTemplateOf(int32_t questId);
};

} // namespace aion::gameserver::questEngine::handlers::template_
