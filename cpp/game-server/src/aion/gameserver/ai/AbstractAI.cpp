#include "aion/gameserver/ai/AbstractAI.h"

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/event/AIEventLog.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::ai {

AbstractAI::AbstractAI(model::gameobjects::Creature& ownerValue)
	: runtime::OwnedPart(ownerValue), owner(ownerValue), currentState(AIState::CREATED), currentSubState(AISubState::NONE) {
}

AbstractAI::~AbstractAI() = default;

std::string AbstractAI::getName() {
	// Java: getClass().getAnnotation(AIName.class) == null ? "noname" : annotation.value(); C++: registryEntry->name
	AION_UNPORTED();
}

bool AbstractAI::canHandleEvent(event::AIEventType eventType) {
	AION_UNPORTED();
}

bool AbstractAI::setStateIfNot(AIState newState) {
	AION_UNPORTED();
}

bool AbstractAI::setSubStateIfNot(AISubState newSubState) {
	AION_UNPORTED();
}

void AbstractAI::onGeneralEvent(event::AIEventType event) {
	AION_UNPORTED();
}

void AbstractAI::onCreatureEvent(event::AIEventType event, model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

void AbstractAI::onCustomEvent(int32_t eventId, std::initializer_list<std::any> args) {
	AION_UNPORTED();
}

int32_t AbstractAI::getObjectId() {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldPosition> AbstractAI::getPosition() {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::getTarget() {
	AION_UNPORTED();
}

bool AbstractAI::isDead() {
	AION_UNPORTED();
}

bool AbstractAI::setThinking() {
	AION_UNPORTED();
}

void AbstractAI::unsetThinking() {
	AION_UNPORTED();
}

void AbstractAI::handleGeneralEvent(event::AIEventType event) {
	AION_UNPORTED();
}

void AbstractAI::logEvent(event::AIEventType event) {
	AION_UNPORTED();
}

void AbstractAI::handleCreatureEvent(event::AIEventType event, model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::spawn(int32_t npcId, float x, float y, float z, int8_t heading) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::spawn(int32_t npcId, float x, float y, float z, int8_t heading, int32_t staticId) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::spawn(int32_t npcId, float x, float y, float z, int8_t heading, int32_t staticId,
	std::optional<std::string_view> aiName) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::rndSpawnInRange(int32_t npcId, float distance) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::rndSpawnInRange(int32_t npcId, float minDistance, float maxDistance) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::ai
