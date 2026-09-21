#include "aion/gameserver/services/event/Event.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/services/event/EventBuffHandler.h"

namespace aion::gameserver::services::event {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.event.Event");

Event::Event(const model::templates::event::EventTemplate* value)
	: eventTemplate(value) {
}

runtime::Ref<Event> Event::create(const model::templates::event::EventTemplate* value) {
	return runtime::makeRef<Event>(value);
}

const skillengine::model::Effect_ForceType* Event::getOrCreateEffectForceType(std::string_view identifier) {
	AION_UNPORTED();
}

bool Event::isEventEffectForceType(const skillengine::model::Effect_ForceType* forceType) {
	return forceType != nullptr && forceType->getName().starts_with(EFFECT_FORCE_TYPE_PREFIX);
}

// lambda at Event.java:102 (fieldmap key event.Event@L102:76)
void Event::start() {
	AION_UNPORTED();
}

void Event::stop() {
	AION_UNPORTED();
}

const skillengine::model::Effect_ForceType* Event::getEffectForceType() {
	AION_UNPORTED();
}

void Event::onTimeChanged(std::chrono::sys_time<std::chrono::milliseconds> now) {
	AION_UNPORTED();
}

void Event::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void Event::onEnteredTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

void Event::onLeftTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

void Event::onEnterMap(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void Event::onPveKill(model::gameobjects::player::Player& killer, model::gameobjects::Npc& victim) {
	AION_UNPORTED();
}

void Event::onPvpKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

void Event::startOrMaintainQuests(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool Event::isAllowedToStartEventQuest(model::gameobjects::player::Player& player, int32_t questId) {
	AION_UNPORTED();
}

void Event::despawnNonEventSpawns(int32_t npcId, world::WorldMap& worldMap) {
	AION_UNPORTED();
}

bool Event::addOnEventEndTask(runtime::PinnedCallback<void()> task) {
	AION_UNPORTED();
}

Event::~Event() = default;

} // namespace aion::gameserver::services::event
