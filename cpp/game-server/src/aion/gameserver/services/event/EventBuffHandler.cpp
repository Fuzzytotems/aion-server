#include "aion/gameserver/services/event/EventBuffHandler.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::event {

EventBuffHandler::EventBuffHandler(std::string_view value, const std::vector<const model::templates::event::Buff*>& buffsValue)
	: eventName(std::string(value)), effectForceType() {
	// Java: this.buffs = buffs; this.effectForceType = Event.getOrCreateEffectForceType(eventName); initBuffData()
	AION_UNPORTED();
}

runtime::Ref<EventBuffHandler> EventBuffHandler::create(std::string_view value, const std::vector<const model::templates::event::Buff*>& buffsValue) {
	return runtime::makeRef<EventBuffHandler>(value, buffsValue);
}

void EventBuffHandler::initBuffData() {
	AION_UNPORTED();
}

void EventBuffHandler::updateActiveBuffSkillIds() {
	AION_UNPORTED();
}

void EventBuffHandler::updateAllowedBuffDays(int32_t endOfMonthDay) {
	AION_UNPORTED();
}

void EventBuffHandler::storeBuffDataInDb() {
	AION_UNPORTED();
}

void EventBuffHandler::resetTodaysBuffs() {
	AION_UNPORTED();
}

void EventBuffHandler::onEventStop() {
	AION_UNPORTED();
}

void EventBuffHandler::onEnterMap(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void EventBuffHandler::onEnteredTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

void EventBuffHandler::onLeftTeam(model::gameobjects::player::Player& player, model::team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

void EventBuffHandler::onPveKill(model::gameobjects::player::Player& killer, model::gameobjects::Npc& victim) {
	AION_UNPORTED();
}

void EventBuffHandler::onPvpKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

void EventBuffHandler::endEventBuffs(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void EventBuffHandler::endRestrictedEventBuffs(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool EventBuffHandler::applyOnTeam(model::gameobjects::player::Player& player,
	const std::function<void(model::gameobjects::player::Player&)>& memberAction) {
	AION_UNPORTED();
}

void EventBuffHandler::tryBuff(model::gameobjects::player::Player& player, model::templates::event::Buff_TriggerCondition triggerCondition) {
	AION_UNPORTED();
}

void EventBuffHandler::tryBuff(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player,
	model::templates::event::Buff_TriggerCondition triggerCondition) {
	AION_UNPORTED();
}

std::unordered_set<int32_t> EventBuffHandler::getActiveBuffSkillIds(const model::templates::event::Buff* buff) {
	AION_UNPORTED();
}

bool EventBuffHandler::canReceiveBuff(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player,
	model::templates::event::Buff_TriggerCondition triggerCondition) {
	AION_UNPORTED();
}

const model::templates::event::Buff::Trigger* EventBuffHandler::findBuffTrigger(const model::templates::event::Buff* buff,
	model::templates::event::Buff_TriggerCondition triggerCondition) {
	AION_UNPORTED();
}

bool EventBuffHandler::checkRestrictions(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool EventBuffHandler::isAllowedTeamSize(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool EventBuffHandler::isAllowedToday(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool EventBuffHandler::isAllowedOnCurrentMap(const model::templates::event::Buff* buff, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool EventBuffHandler::checkInstanceLevel(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void EventBuffHandler::onTimeChanged(std::chrono::sys_time<std::chrono::milliseconds> now) {
	AION_UNPORTED();
}

EventBuffHandler::~EventBuffHandler() = default;

} // namespace aion::gameserver::services::event
