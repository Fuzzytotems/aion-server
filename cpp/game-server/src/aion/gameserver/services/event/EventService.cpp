#include "aion/gameserver/services/event/EventService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/event/Event.h"

namespace aion::gameserver::services::event {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.event.EventService");

EventService::EventService() = default;

EventService::~EventService() = default;

EventService& EventService::getInstance() {
	static EventService instance; // Java SingletonHolder
	return instance;
}

// callback at EventService.java:51 (fieldmap key EventService@L51:50)
bool EventService::start() {
	AION_UNPORTED();
}

void EventService::stop() {
	AION_UNPORTED();
}

void EventService::validateConfiguredEventNames() {
	AION_UNPORTED();
}

void EventService::checkActiveEvents() {
	AION_UNPORTED();
}

bool EventService::isInactiveEventForceType(const skillengine::model::Effect_ForceType* forceType) {
	AION_UNPORTED();
}

void EventService::onTimeChanged() {
	AION_UNPORTED();
}

void EventService::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void EventService::onEnteredTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

void EventService::onLeftTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

void EventService::onEnterMap(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void EventService::onPveKill(model::gameobjects::player::Player& killer, model::gameobjects::Npc& victim) {
	AION_UNPORTED();
}

void EventService::onPvpKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

bool EventService::isAllEvents(const std::unordered_set<std::string>& list) {
	AION_UNPORTED();
}

runtime::Ref<runtime::RcHashSet<runtime::Ref<Event>>> EventService::collectActiveEvents() {
	AION_UNPORTED();
}

runtime::Ref<Event> EventService::findOrCreateEvent(const model::templates::event::EventTemplate* et) {
	AION_UNPORTED();
}

void EventService::startOrStopEvents(runtime::RcHashSet<runtime::Ref<Event>>& oldActiveEvents, runtime::RcHashSet<runtime::Ref<Event>>& newActiveEvents) {
	AION_UNPORTED();
}

runtime::Ref<runtime::RcHashSet<int32_t>> EventService::collectQuestIds(runtime::RcHashSet<runtime::Ref<Event>>& events) {
	AION_UNPORTED();
}

runtime::Ref<runtime::RcArrayList<const model::templates::globaldrops::GlobalRule*>> EventService::collectDropRules(runtime::RcHashSet<runtime::Ref<Event>>& events) {
	AION_UNPORTED();
}

bool EventService::isEventActive(std::string_view eventName) {
	AION_UNPORTED();
}

bool EventService::isActiveEventQuest(int32_t questId) {
	AION_UNPORTED();
}

commons::configuration::Properties EventService::getActiveEventConfigProperties() {
	AION_UNPORTED();
}

void EventService::updateEventTheme() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::event
