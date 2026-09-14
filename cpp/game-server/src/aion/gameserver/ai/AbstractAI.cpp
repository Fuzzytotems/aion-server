#include "aion/gameserver/ai/AbstractAI.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

// Member types (docs/design/hub-headers.md §3.3): the destructor releases `eventLog` and needs the complete AIEventLog (not a hub; its declaration
// header is an S0c item of chunk P5-05). Remove the guard in the change that adds the header (a freeze gate: skeleton.py --guards --freeze).
#if __has_include("aion/gameserver/ai/event/AIEventLog.h")
#define AION_S0B_ABSTRACT_AI_OWNER 1
#include "aion/gameserver/ai/event/AIEventLog.h"
#else
#define AION_S0B_ABSTRACT_AI_OWNER 0
#endif

namespace aion::gameserver::ai {

#if AION_S0B_ABSTRACT_AI_OWNER
AbstractAI::AbstractAI(model::gameobjects::Creature& ownerValue)
	: runtime::OwnedPart(ownerValue), owner(ownerValue), currentState(AIState::CREATED), currentSubState(AISubState::NONE) {
}

AbstractAI::~AbstractAI() = default;
#endif

std::string AbstractAI::getName() {
	// Java: getClass().getAnnotation(AIName.class) == null ? "noname" : annotation.value(); C++: registryEntry->name
	AION_UNPORTED();
}

bool AbstractAI::canHandleEvent(event::AIEventType eventType) {
	AION_UNPORTED();
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
bool AbstractAI::setStateIfNot(AIState newState) {
	AION_UNPORTED();
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
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

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
bool AbstractAI::setThinking() {
	AION_UNPORTED();
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
void AbstractAI::unsetThinking() {
	AION_UNPORTED();
}

void AbstractAI::handleGeneralEvent(event::AIEventType event) {
	AION_UNPORTED();
}

// lint: L7 Java synchronized (this) block; the ported body keeps it as SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
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
