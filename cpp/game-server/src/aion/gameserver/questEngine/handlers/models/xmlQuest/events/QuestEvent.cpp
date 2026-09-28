#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/QuestEvent.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

const std::vector<int32_t>& QuestEvent::getIds() const {
	static const std::vector<int32_t> EMPTY; // Java: `ids = new ArrayList<>()` on first access of an absent ids attribute
	return ids ? *ids : EMPTY;
}

bool QuestEvent::operate(model::QuestEnv& /*env*/) const {
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events
