#include "aion/gameserver/world/zone/handler/QuestZoneHandler.h"

#include "aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::world::zone::handler {

QuestZoneHandler::QuestZoneHandler(int32_t questIdValue) : questId(questIdValue) {
	if (questId == 0 || dataholders::DataManager::QUEST_DATA->getQuestById(questId) == nullptr) {
		// Java: throw new IncompleteAnnotationException(ZoneNameAnnotation.class, "questId") (a RuntimeException with this message)
		throw runtime::IllegalStateException("com.aionemu.gameserver.world.zone.handler.ZoneNameAnnotation missing element questId");
	}
}

QuestZoneHandler::~QuestZoneHandler() = default;

void QuestZoneHandler::onEnterZone(model::gameobjects::Creature& creature, ZoneInstance& zone) {
	AION_UNPORTED();
}

void QuestZoneHandler::onLeaveZone(model::gameobjects::Creature& creature, ZoneInstance& zone) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::zone::handler
