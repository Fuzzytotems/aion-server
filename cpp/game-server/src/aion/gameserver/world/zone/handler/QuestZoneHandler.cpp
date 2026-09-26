#include "aion/gameserver/world/zone/handler/QuestZoneHandler.h"

#include "aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::world::zone::handler {

QuestZoneHandler::QuestZoneHandler(int32_t questIdValue) : questId(questIdValue) {
	if (questId == 0 || dataholders::DataManager::QUEST_DATA->getQuestById(questId) == nullptr) {
		// Java: throw new IncompleteAnnotationException(ZoneNameAnnotation.class, "questId") (a RuntimeException with this message)
		throw runtime::IllegalStateException("com.aionemu.gameserver.world.zone.handler.ZoneNameAnnotation missing element questId");
	}
}

QuestZoneHandler::~QuestZoneHandler() = default;

void QuestZoneHandler::onEnterZone(model::gameobjects::Creature& creature, ZoneInstance& zone) {
	auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature);
	if (player == nullptr)
		return;
	runtime::Ref<controllers::observer::AbstractQuestZoneObserver> observer = createObserver(*player, zone.getZoneTemplate());
	creature.getObserveController()->addObserver(*observer);
	observed.put(creature.getObjectId(), observer);
}

void QuestZoneHandler::onLeaveZone(model::gameobjects::Creature& creature, ZoneInstance& zone) {
	if (dynamic_cast<model::gameobjects::player::Player*>(&creature) == nullptr)
		return;
	runtime::Ptr<controllers::observer::AbstractQuestZoneObserver> observer = observed.get(creature.getObjectId());
	if (observer) {
		creature.getObserveController()->removeObserver(*observer);
		observed.remove(creature.getObjectId());
	}
}

} // namespace aion::gameserver::world::zone::handler
