#include "aion/gameserver/world/zone/handler/QuestZoneHandler.h"

#include "aion/gameserver/runtime/base/Unported.h"

// Member types (docs/design/hub-headers.md §3.3): the constructor and the destructor instantiate the destructor of `observed`, which releases
// Ref<AbstractQuestZoneObserver> and needs the complete observer type (not a hub; S0c declaration header of the controllers chunk, §3.5).
// Remove the guard in the change that adds the header.
#if __has_include("aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h")
#define AION_S0B_QUEST_ZONE_HANDLER_OBSERVER 1
#include "aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h"
#else
#define AION_S0B_QUEST_ZONE_HANDLER_OBSERVER 0
#endif

namespace aion::gameserver::world::zone::handler {

#if AION_S0B_QUEST_ZONE_HANDLER_OBSERVER
QuestZoneHandler::QuestZoneHandler(int32_t questIdValue) : questId(questIdValue) {
	// Java: throw new IncompleteAnnotationException(ZoneNameAnnotation.class, "questId") if questId == 0 or QUEST_DATA has no such quest
	AION_UNPORTED();
}

QuestZoneHandler::~QuestZoneHandler() = default;
#endif

void QuestZoneHandler::onEnterZone(model::gameobjects::Creature& creature, ZoneInstance& zone) {
	AION_UNPORTED();
}

void QuestZoneHandler::onLeaveZone(model::gameobjects::Creature& creature, ZoneInstance& zone) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::zone::handler
