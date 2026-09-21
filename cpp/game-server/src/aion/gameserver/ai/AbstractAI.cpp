#include "aion/gameserver/ai/AbstractAI.h"

#include <cmath>
#include <numbers>
#include <span>
#include <stacktrace>
#include <string>
#include <typeinfo>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AIStateInfo.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/event/AIEventLog.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/FreezeEventHandler.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AggroTarget.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::ai {

namespace {

std::string nameOf(AIState state) {
	return std::string(xml::enumName(state));
}

std::string nameOf(AISubState subState) {
	return std::string(xml::enumName(subState));
}

std::string nameOf(event::AIEventType event) {
	return std::string(xml::enumName(event));
}

/** Java: (Player) creature for DIALOG_START / DIALOG_FINISH (ClassCastException for another creature) */
model::gameobjects::player::Player& asPlayer(model::gameobjects::Creature& creature) {
	auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature);
	if (player == nullptr) // Java's message names the runtime class: "class <FQN> cannot be cast to class ...Player"
		throw runtime::ClassCastException(std::string(typeid(creature).name()) + " cannot be cast to class com.aionemu.gameserver.model.gameobjects.player.Player");
	return *player;
}

} // namespace

AbstractAI::AbstractAI(model::gameobjects::Creature& ownerValue)
	: runtime::OwnedPart(ownerValue), owner(ownerValue), currentState(AIState::CREATED), currentSubState(AISubState::NONE) {
}

AbstractAI::~AbstractAI() = default;

std::string AbstractAI::getName() {
	// Java: getClass().getAnnotation(AIName.class) == null ? "noname" : annotation.value(); C++: the registry entry newAI created the AI from
	const handlers::AIHandlerEntry* entry = registryEntry.get();
	return entry == nullptr ? "noname" : std::string(entry->name);
}

bool AbstractAI::canHandleEvent(event::AIEventType eventType) {
	switch (eventType) {
		case event::AIEventType::CREATURE_MOVED:
		case event::AIEventType::DIALOG_START:
		case event::AIEventType::DIALOG_FINISH:
			return currentState.get() == AIState::IDLE || currentState.get() == AIState::WALKING;
		default:
			break;
	}
	return true;
}

bool AbstractAI::setStateIfNot(AIState newState) {
	SYNCHRONIZED(*this) {
		if (currentState.get() == newState)
			return false;

		if (isLogging()) {
			AILogger::info(*this, "Setting AI state to " + nameOf(newState));
			if (currentState.get() == AIState::DIED && newState == AIState::FIGHT) {
				// Java: logs every element of new Throwable().getStackTrace(); C++: one entry per line of the current stack trace
				std::string stack = std::to_string(std::stacktrace::current());
				size_t start = 0;
				while (start < stack.size()) {
					size_t end = stack.find('\n', start);
					if (end == std::string::npos)
						end = stack.size();
					AILogger::info(*this, std::string_view(stack).substr(start, end - start));
					start = end + 1;
				}
			}
		}
		currentState.set(newState);
	}
	return true;
}

bool AbstractAI::setSubStateIfNot(AISubState newSubState) {
	SYNCHRONIZED(*this) {
		if (currentSubState.get() == newSubState) {
			if (isLogging()) {
				AILogger::info(*this, "Can't change substate to " + nameOf(newSubState) + " from " + nameOf(currentSubState.get()));
			}
			return false;
		}
		if (isLogging()) {
			AILogger::info(*this, "Setting AI substate to " + nameOf(newSubState));
		}
		currentSubState.set(newSubState);
	}
	return true;
}

void AbstractAI::onGeneralEvent(event::AIEventType event) {
	if (canHandle(currentState.get(), event) && canHandleEvent(event)) {
		if (isLogging()) {
			AILogger::info(*this, "General event " + nameOf(event));
		}
		handleGeneralEvent(event);
	}
}

void AbstractAI::onCreatureEvent(event::AIEventType event, model::gameobjects::Creature& creature) {
	// Java: Objects.requireNonNull(creature, "Creature must not be null") (a reference is never null)
	if (canHandle(currentState.get(), event) && canHandleEvent(event)) {
		if (isLogging()) {
			AILogger::info(*this, "Creature event " + nameOf(event) + ": " + std::to_string(creature.getObjectTemplate()->getTemplateId()));
		}
		// Java: try { ... } finally { DEPTH.set(0); }
		struct DepthReset {
			~DepthReset() { DEPTH = 0; }
		} depthReset;
		int32_t depth = DEPTH.value_or(0);
		if (depth > 20) {
			runtime::Ptr<model::gameobjects::Creature> mostHated = owner.getAggroList().getTarget(controllers::attack::AggroTarget::MOST_HATED);
			// Java: throw new StackOverflowError(...) (an Error; no C++ counterpart, so the commons base exception carries the message)
			throw commons::utils::Exception("Aborted abnormal AI event recursion for " + owner.toString() + " with AIEventType." + nameOf(event) +
				" and target: " + creature.toString() + ", most hated: " + (mostHated ? mostHated->toString() : "null"));
		}
		DEPTH = depth + 1;
		handleCreatureEvent(event, creature);
	}
}

void AbstractAI::onCustomEvent(int32_t eventId, std::initializer_list<std::any> args) {
	if (isLogging()) {
		AILogger::info(*this, "Custom event - id = " + std::to_string(eventId));
	}
	handleCustomEvent(eventId, std::span<const std::any>(args.begin(), args.size()));
}

int32_t AbstractAI::getObjectId() {
	return owner.getObjectId();
}

runtime::Ptr<world::WorldPosition> AbstractAI::getPosition() {
	return owner.getPosition();
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::getTarget() {
	return owner.getTarget();
}

bool AbstractAI::isDead() {
	return owner.isDead();
}

bool AbstractAI::setThinking() {
	SYNCHRONIZED(*this) {
		if (thinking.get())
			return false;
		thinking.set(true);
	}
	return true;
}

void AbstractAI::unsetThinking() {
	SYNCHRONIZED(*this) {
		thinking.set(false);
	}
}

void AbstractAI::handleGeneralEvent(event::AIEventType event) {
	if (isLogging()) {
		AILogger::info(*this, "Handle general event " + nameOf(event));
	}
	logEvent(event);

	switch (event) {
		case event::AIEventType::MOVE_VALIDATE:
			handleMoveValidate();
			break;
		case event::AIEventType::MOVE_ARRIVED:
			handleMoveArrived();
			break;
		case event::AIEventType::SPAWNED:
			handleSpawned();
			break;
		case event::AIEventType::BEFORE_SPAWNED:
			handleBeforeSpawned();
			break;
		case event::AIEventType::DESPAWNED:
			handleDespawned();
			break;
		case event::AIEventType::DIED:
			handleDied();
			break;
		case event::AIEventType::ATTACK_COMPLETE:
			handleAttackComplete();
			break;
		case event::AIEventType::ATTACK_FINISH:
			handleFinishAttack();
			break;
		case event::AIEventType::TARGET_TOOFAR:
			handleTargetTooFar();
			break;
		case event::AIEventType::TARGET_GIVEUP:
			handleTargetGiveup();
			break;
		case event::AIEventType::NOT_AT_HOME:
			handleNotAtHome();
			break;
		case event::AIEventType::BACK_HOME:
			handleBackHome();
			break;
		case event::AIEventType::ACTIVATE:
			handleActivate();
			break;
		case event::AIEventType::DEACTIVATE:
			handleDeactivate();
			break;
		case event::AIEventType::FREEZE:
			handler::FreezeEventHandler::onFreeze(*this);
			break;
		case event::AIEventType::UNFREEZE:
			handler::FreezeEventHandler::onUnfreeze(*this);
			break;
		case event::AIEventType::DROP_REGISTERED:
			handleDropRegistered();
			break;
		default:
			break;
	}
}

void AbstractAI::logEvent(event::AIEventType event) {
	if (configs::main::AIConfig::EVENT_DEBUG.load()) {
		if (!eventLog.get()) {
			SYNCHRONIZED(*this) {
				if (!eventLog.get()) {
					eventLog.set(event::AIEventLog::create(10));
				}
			}
		}
		eventLog.get()->addFirst(event);
	}
}

void AbstractAI::handleCreatureEvent(event::AIEventType event, model::gameobjects::Creature& creature) {
	switch (event) {
		case event::AIEventType::ATTACK:
			handleAttack(creature);
			logEvent(event);
			break;
		case event::AIEventType::CREATURE_NEEDS_SUPPORT:
			if (!handleCreatureNeedsSupport(creature))
				handleCreatureNeedsSupportByGuard(creature);
			logEvent(event);
			break;
		case event::AIEventType::CREATURE_NEEDS_HELP:
			creatureNeedsHelp(creature);
			break;
		case event::AIEventType::CREATURE_SEE:
			handleCreatureSee(creature);
			break;
		case event::AIEventType::CREATURE_NOT_SEE:
			handleCreatureNotSee(creature);
			break;
		case event::AIEventType::CREATURE_MOVED:
			handleCreatureMoved(creature);
			break;
		case event::AIEventType::CREATURE_AGGRO:
			handleCreatureAggro(creature);
			logEvent(event);
			break;
		case event::AIEventType::TARGET_CHANGED:
			handleTargetChanged(creature);
			break;
		case event::AIEventType::FOLLOW_ME:
			handleFollowMe(creature);
			logEvent(event);
			break;
		case event::AIEventType::STOP_FOLLOW_ME:
			handleStopFollowMe(creature);
			logEvent(event);
			break;
		case event::AIEventType::DIALOG_START:
			handleDialogStart(asPlayer(creature));
			logEvent(event);
			break;
		case event::AIEventType::DIALOG_FINISH:
			handleDialogFinish(asPlayer(creature));
			logEvent(event);
			break;
		default:
			break;
	}
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::spawn(int32_t npcId, float x, float y, float z, int8_t heading) {
	return spawn(npcId, x, y, z, heading, 0);
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::spawn(int32_t npcId, float x, float y, float z, int8_t heading, int32_t staticId) {
	return spawn(npcId, x, y, z, heading, staticId, std::nullopt);
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::spawn(int32_t npcId, float x, float y, float z, int8_t heading, int32_t staticId,
	std::optional<std::string_view> aiName) {
	runtime::Ref<model::templates::spawns::SpawnTemplate> template_ =
		spawnengine::SpawnEngine::newSingleTimeSpawn(owner.getWorldId(), npcId, x, y, z, heading, runtime::Ptr<model::gameobjects::VisibleObject>(owner), aiName);
	template_->setStaticId(staticId);
	return spawnengine::SpawnEngine::spawnObject(*template_, owner.getInstanceId());
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::rndSpawnInRange(int32_t npcId, float distance) {
	// Java: Math.toRadians(Rnd.nextFloat(360f)) (angdeg / 180.0 * PI)
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) / 180.0 * std::numbers::pi;
	runtime::Ptr<world::WorldPosition> p = getPosition();
	float x = p->getX() + static_cast<float>(std::cos(angleRadians) * distance);
	float y = p->getY() + static_cast<float>(std::sin(angleRadians) * distance);
	geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(owner, x, y, p->getZ());
	return spawn(npcId, pos.getX(), pos.getY(), pos.getZ(), p->getHeading());
}

runtime::Ptr<model::gameobjects::VisibleObject> AbstractAI::rndSpawnInRange(int32_t npcId, float minDistance, float maxDistance) {
	return rndSpawnInRange(npcId, commons::utils::Rnd::nextFloat(minDistance, maxDistance));
}

} // namespace aion::gameserver::ai
