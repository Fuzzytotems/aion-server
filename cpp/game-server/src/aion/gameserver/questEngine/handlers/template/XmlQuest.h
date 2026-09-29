#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/fwd.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.XmlQuest: the `xml_quest` XML quests (only 1127 "Ancient Cube" in the data) -
 * scripted in the xmlQuest language: the <on_talk_event> and <on_kill_event> elements run first, the start and reward pages after them.
 * <p>
 * C++ differences:
 * - A Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null).
 * - `onTalkEvents`/`onKillEvents` point to the event elements of the XmlQuestData that built the handler (Java: the same objects; static data,
 *   alive for the process). An absent list is the bound empty vector, which Java's `if (onTalkEvents != null)` skips the same way.
 *
 * @author Mr.Poke, Bobobear, Pad
 */
class XmlQuest : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(XmlQuest::startNpcIds)};
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(XmlQuest::endNpcIds)};
	runtime::ArrayList<const models::xmlQuest::events::OnTalkEvent*> onTalkEvents{AION_LOCK_CLASS(XmlQuest::onTalkEvents)};
	runtime::ArrayList<const models::xmlQuest::events::OnKillEvent*> onKillEvents{AION_LOCK_CLASS(XmlQuest::onKillEvents)};
	const bool isDataDriven;

public:
	XmlQuest(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds, const std::optional<std::vector<int32_t>>& endNpcIds,
		const std::vector<models::xmlQuest::events::OnTalkEvent>& onTalkEvents, const std::vector<models::xmlQuest::events::OnKillEvent>& onKillEvents);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	bool onKillEvent(model::QuestEnv& env) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
