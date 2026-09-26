#include "aion/gameserver/controllers/movement/NpcMoveController.h"

#include <cmath>
#include <format>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/handler/TargetEventHandler.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate_LoopType.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/WalkerFormator.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::controllers::movement {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.movement.NpcMoveController");

using ai::AILogger;
using ai::AIState;
using ai::AISubState;
using ai::handler::TargetEventHandler;
using ai::manager::WalkManager;
using geoEngine::math::JavaFloat;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::state::CreatureState;
using runtime::Ptr;
using runtime::Ref;
using utils::PositionUtil;

namespace {

/** Java: Math.toRadians(angdeg) (JDK 9+ constant) */
double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

std::string_view destinationName(NpcMoveController_Destination destination) noexcept {
	switch (destination) {
		case NpcMoveController_Destination::TARGET_OBJECT:
			return "TARGET_OBJECT";
		case NpcMoveController_Destination::POINT:
			return "POINT";
		case NpcMoveController_Destination::FORCED_POINT:
			return "FORCED_POINT";
	}
	return "";
}

} // namespace

NpcMoveController::NpcMoveController(model::gameobjects::Npc& ownerValue) : CreatureMoveController(ownerValue) {
}

NpcMoveController::~NpcMoveController() = default;

void NpcMoveController::moveToTargetObject() {
	Npc& npc = static_cast<Npc&>(owner);
	if (started->compareAndSet(false, true)) {
		if (npc.getAi().isLogging()) {
			AILogger::moveinfo(npc, "MC: moveToTarget started");
		}
		destination = Destination::TARGET_OBJECT;
		updateLastMove();
		npc.getController().onStartMove();
	}
}

bool NpcMoveController::moveToPoint(float x, float y, float z) {
	Npc& npc = static_cast<Npc&>(owner);
	bool startedMoving = started->compareAndSet(false, true);
	if (!startedMoving && destination.get() != Destination::POINT)
		return false;
	if (npc.getAi().isLogging()) {
		AILogger::moveinfo(npc, std::format("MC: moveToPoint (startedMoving={})", startedMoving));
	}
	// java-race: destination and point coordinates are written without a lock after the compare-and-set; concurrent moveToPoint and
	// forcedMoveToPoint callers can leave a destination and coordinates mixed from two calls
	destination = Destination::POINT;
	pointX = x;
	pointY = y;
	pointZ = z;
	updateLastMove();
	if (startedMoving)
		npc.getController().onStartMove();
	return true;
}

void NpcMoveController::forcedMoveToPoint(float x, float y, float z) {
	Npc& npc = static_cast<Npc&>(owner);
	if (started->compareAndSet(false, true)) {
		if (npc.getAi().isLogging()) {
			AILogger::moveinfo(npc, "MC: forcedMoveToPoint started");
		}
		// java-race: see moveToPoint (a concurrent moveToPoint of a started move writes the same fields)
		destination = Destination::FORCED_POINT;
		pointX = x;
		pointY = y;
		pointZ = z;
		updateLastMove();
		npc.getController().onStartMove();
	}
}

void NpcMoveController::moveToNextPoint() {
	Npc& npc = static_cast<Npc&>(owner);
	if (started->compareAndSet(false, true)) {
		if (npc.getAi().isLogging()) {
			AILogger::moveinfo(npc, "MC: moveToNextPoint started");
		}
		destination = Destination::POINT;
		updateLastMove();
		npc.getController().onStartMove();
	}
}

void NpcMoveController::moveToDestination() {
	Npc& npc = static_cast<Npc&>(owner);
	if (npc.getAi().isLogging()) {
		AILogger::moveinfo(npc, std::string("moveToDestination destination: ").append(destinationName(destination.get())));
	}
	if (npc.isDead()) {
		abortMove();
		return;
	}
	if (!npc.canPerformMove()) {
		if (npc.getAi().isLogging()) {
			AILogger::moveinfo(npc, "moveToDestination can't perform move");
		}
		if (started->compareAndSet(true, false)) {
			setAndSendStopMove(npc);
			updateLastMove();
		}
		return;
	}
	if (started->compareAndSet(false, true)) {
		updateLastMove();
		setAndSendStartMove(npc);
	}

	switch (destination.get()) {
		case Destination::TARGET_OBJECT: {
			Ptr<VisibleObject> target = npc.getTarget(); // todo no target
			if (!target)
				return;
			if (!PositionUtil::isInRange(*target, pointX.get(), pointY.get(), pointZ.get(), MOVE_CHECK_OFFSET)) {
				Ptr<Creature> creature;
				if (configs::main::GeoDataConfig::GEO_NPC_MOVE && !npc.isInFlyingState() && (creature = runtime::as<Creature>(target)) &&
					(nextPointFromGeo.get() || (nextPointFromGeo = !isOnGround(*creature)).get())) {
					if (trySetValidGeoPoint(target->getX(), target->getY()) && nextPointFromGeo.get())
						nextPointFromGeo = !isOnGround(*creature);
				} else {
					pointX = target->getX();
					pointY = target->getY();
					pointZ = target->getZ();
				}
			}
			moveToLocation(pointX.get(), pointY.get(), pointZ.get());
			break;
		}
		case Destination::POINT:
		case Destination::FORCED_POINT:
			moveToLocation(pointX.get(), pointY.get(), pointZ.get());
			break;
	}
	updateLastMove();
}

bool NpcMoveController::isOnGround(model::gameobjects::Creature& creature) {
	return !creature.isFlying() && !creature.getMoveController()->isJumping() &&
		(creature.getMoveController()->getMovementMask() & MovementMask::FALL) == 0;
}

bool NpcMoveController::trySetValidGeoPoint(float targetX, float targetY) {
	Npc& npc = static_cast<Npc&>(owner);
	if (pointX.get() == 0 && pointY.get() == 0 && pointZ.get() == 0) {
		pointX = npc.getX();
		pointY = npc.getY();
		pointZ = npc.getZ();
		npc.getGameStats()->setNextGeoZUpdate(0);
	}
	int64_t nowMillis = commons::utils::currentTimeMillis();
	if (nowMillis < npc.getGameStats()->getNextGeoZUpdate())
		return false;
	float distance2D = static_cast<float>(PositionUtil::getDistance(pointX.get(), pointY.get(), targetX, targetY));
	if (distance2D < MOVE_CHECK_OFFSET)
		return false; // no need to recalculate
	if (distance2D > MAX_GEO_POINT_DISTANCE) {
		double angleRadians = toRadians(PositionUtil::calculateAngleFrom(pointX.get(), pointY.get(), targetX, targetY));
		targetX = pointX.get() + static_cast<float>(std::cos(angleRadians) * MAX_GEO_POINT_DISTANCE);
		targetY = pointY.get() + static_cast<float>(std::sin(angleRadians) * MAX_GEO_POINT_DISTANCE);
		distance2D = MAX_GEO_POINT_DISTANCE;
	}
	float maxZDiff = distance2D + MOVE_CHECK_OFFSET;
	float geoZ = world::geo::GeoService::getInstance().getZ(npc.getWorldId(), targetX, targetY, pointZ.get() + maxZDiff, pointZ.get() - maxZDiff,
		npc.getInstanceId());
	if (std::isnan(geoZ)) {
		npc.getGameStats()->setNextGeoZUpdate(nowMillis + 1000);
		return false;
	}
	pointX = targetX;
	pointY = targetY;
	pointZ = geoZ;
	npc.getGameStats()->setNextGeoZUpdate(nowMillis + 500);
	return true;
}

void NpcMoveController::moveToLocation(float targetX, float targetY, float targetZ) {
	Npc& npc = static_cast<Npc&>(owner);
	float ownerX = npc.getX();
	float ownerY = npc.getY();
	float ownerZ = npc.getZ();

	if (npc.getAi().isLogging()) {
		AILogger::moveinfo(npc, std::format("OLD targetDestX: {} targetDestY: {} targetDestZ {}", JavaFloat::toString(targetDestX.get()),
			JavaFloat::toString(targetDestY.get()), JavaFloat::toString(targetDestZ.get())));
	}

	// to prevent broken walkers in case of activating/deactivating zones
	if (targetX == 0 && targetY == 0) {
		targetX = npc.getSpawn()->getX();
		targetY = npc.getSpawn()->getY();
		targetZ = npc.getSpawn()->getZ();
		clearBackSteps();
	} else if (npc.getAi().getState() == AIState::FIGHT || npc.getAi().getState() == AIState::FOLLOWING) {
		tryStoreStep(targetX, targetY, targetZ);
	}

	bool destinationChanged = targetX != targetDestX.get() || targetY != targetDestY.get() || targetZ != targetDestZ.get();
	if (targetX != targetDestX.get() || targetY != targetDestY.get())
		heading = PositionUtil::getHeadingTowards(ownerX, ownerY, targetX, targetY);

	targetDestX = targetX;
	targetDestY = targetY;
	targetDestZ = targetZ;

	if (npc.getAi().isLogging()) {
		AILogger::moveinfo(npc, std::format("ownerX={} ownerY={} ownerZ={}", JavaFloat::toString(ownerX), JavaFloat::toString(ownerY),
			JavaFloat::toString(ownerZ)));
		AILogger::moveinfo(npc, std::format("targetDestX: {} targetDestY: {} targetDestZ {}", JavaFloat::toString(targetDestX.get()),
			JavaFloat::toString(targetDestY.get()), JavaFloat::toString(targetDestZ.get())));
	}

	float currentSpeed = npc.getGameStats()->getMovementSpeedFloat();
	float futureDistPassed = currentSpeed * static_cast<float>(commons::utils::currentTimeMillis() - lastMoveUpdate.get()) / 1000.0f;
	float dist = static_cast<float>(PositionUtil::getDistance(ownerX, ownerY, ownerZ, targetX, targetY, targetZ));

	if (npc.getAi().isLogging()) {
		AILogger::moveinfo(npc, std::format("futureDist: {} dist: {}", JavaFloat::toString(futureDistPassed), JavaFloat::toString(dist)));
	}

	if (dist == 0) {
		if (npc.getAi().getState() == AIState::RETURNING) {
			if (npc.getAi().isLogging()) {
				AILogger::moveinfo(npc, "State RETURNING: abort move");
			}
			TargetEventHandler::onTargetReached(*runtime::cast<ai::NpcAI>(npc.getAi()));
		}
		return;
	}

	if (futureDistPassed > dist) {
		futureDistPassed = dist;
	}

	float distFraction = futureDistPassed / dist;
	float newX = (targetDestX.get() - ownerX) * distFraction + ownerX;
	float newY = (targetDestY.get() - ownerY) * distFraction + ownerY;
	float newZ = (targetDestZ.get() - ownerZ) * distFraction + ownerZ;
	if (configs::main::GeoDataConfig::GEO_NPC_MOVE && configs::main::GeoDataConfig::GEO_ENABLE && npc.getAi().getSubState() != AISubState::WALK_PATH &&
		npc.getAi().getState() != AIState::RETURNING && npc.getGameStats()->getNextGeoZUpdate() < commons::utils::currentTimeMillis()) {
		// fix Z if npc doesn't move to spawn point
		if (npc.getSpawn()->getX() != targetDestX.get() || npc.getSpawn()->getY() != targetDestY.get() || npc.getSpawn()->getZ() != targetDestZ.get()) {
			float geoZ = world::geo::GeoService::getInstance().getZ(npc.getWorldId(), newX, newY, newZ + 2, std::min(newZ, ownerZ) - 2, npc.getInstanceId());
			if (!std::isnan(geoZ)) {
				if (std::abs(newZ - geoZ) > 1)
					destinationChanged = true;
				newZ = geoZ;
				bool isXYDestinationReached = PositionUtil::getDistance(newX, newY, pointX.get(), pointY.get()) < MOVE_OFFSET;
				if (isXYDestinationReached && !PositionUtil::isInRange(newX, newY, newZ, pointX.get(), pointY.get(), pointZ.get(), MOVE_OFFSET))
					pointZ = newZ; // original pointZ is unreachable, override it so isReachedPoint() can return true
			}
		}
		npc.getGameStats()->setNextGeoZUpdate(commons::utils::currentTimeMillis() + 1000);
	}
	if (npc.getAi().isLogging()) {
		AILogger::moveinfo(npc, std::format("newX={} newY={} newZ={} mask={}", JavaFloat::toString(newX), JavaFloat::toString(newY),
			JavaFloat::toString(newZ), movementMask.get()));
	}

	world::World::getInstance().updatePosition(npc, newX, newY, newZ, heading.get(), false);

	int8_t newMask = getMoveMask(destinationChanged);
	if (movementMask.get() != newMask || destinationChanged) {
		if (movementMask.get() != newMask) {
			if (npc.getAi().isLogging()) {
				AILogger::moveinfo(npc, std::format("oldMask={} newMask={}", movementMask.get(), newMask));
			}
			movementMask = newMask;
		}
		utils::PacketSendUtility::broadcastPacket(npc, network::aion::serverpackets::SM_MOVE(npc));
	}
}

int8_t NpcMoveController::getMoveMask(bool directionChanged) {
	Npc& npc = static_cast<Npc&>(owner);
	if (directionChanged)
		return MovementMask::NPC_STARTMOVE;
	else if (npc.getAi().getState() == AIState::RETURNING)
		return MovementMask::NPC_RUN_FAST;
	else if (npc.getAi().getState() == AIState::FOLLOWING)
		return MovementMask::NPC_WALK_SLOW;

	int8_t mask = MovementMask::IMMEDIATE;
	const std::unique_ptr<model::stats::calc::Stat2> stat = npc.getGameStats()->getMovementSpeed();
	if (npc.isInState(CreatureState::WEAPON_EQUIPPED)) {
		mask = stat->getBonus() < 0 ? MovementMask::NPC_RUN_FAST : MovementMask::NPC_RUN_SLOW;
	} else if (npc.isInState(CreatureState::WALK_MODE) || npc.isInState(CreatureState::ACTIVE)) {
		mask = stat->getBonus() < 0 ? MovementMask::NPC_WALK_FAST : MovementMask::NPC_WALK_SLOW;
	}
	if (npc.isFlying())
		mask = static_cast<int8_t>(mask | MovementMask::GLIDE);
	return mask;
}

void NpcMoveController::abortMove() {
	if (!started->get())
		return;
	resetMove();
	setAndSendStopMove(static_cast<Npc&>(owner));
}

void NpcMoveController::resetMove() {
	Npc& npc = static_cast<Npc&>(owner);
	if (npc.getAi().isLogging()) {
		AILogger::moveinfo(npc, "MC perform stop");
	}
	npc.getController().onStopMove();
	started->set(false);
	targetDestX = 0;
	targetDestY = 0;
	targetDestZ = 0;
	pointX = 0;
	pointY = 0;
	pointZ = 0;
	nextPointFromGeo = false;
}

void NpcMoveController::setWalkerTemplate(const model::templates::walker::WalkerTemplate* value, int32_t stepIndex) {
	this->walkerTemplate = value;
	if (value == nullptr)
		throw runtime::NullPointerException("NpcMoveController.setWalkerTemplate: walkerTemplate is null"); // Java: walkerTemplate.getRouteStep
	this->currentStep = value->getRouteStep(stepIndex);
}

void NpcMoveController::setRouteStep(const model::templates::walker::RouteStep* step) {
	Npc& npc = static_cast<Npc&>(owner);
	std::optional<model::templates::zone::Point2D> dest;
	if (Ptr<spawnengine::WalkerGroup> walkerGroup = npc.getWalkerGroup()) {
		const model::templates::walker::RouteStep* current = detail::nonNull(currentStep.get(), "currentStep");
		dest = spawnengine::WalkerGroup::getLinePoint(model::templates::zone::Point2D(current->getX(), current->getY()),
			model::templates::zone::Point2D(step->getX(), step->getY()), *walkerGroup->getClusterData(npc));
		this->pointZ = current->getZ();
		walkerGroup->setStep(npc, step->getStepIndex());
	} else {
		this->pointZ = step->getZ();
		this->isStop_ = walkerTemplate.get()->getLoopType() == model::templates::walker::WalkerTemplate_LoopType::NONE && step->isLastStep();
	}
	this->currentStep = step;
	this->pointX = !dest ? step->getX() : dest->getX();
	this->pointY = !dest ? step->getY() : dest->getY();
	this->destination = Destination::POINT;
}

bool NpcMoveController::isReachedPoint() {
	return PositionUtil::isInRange(owner, pointX.get(), pointY.get(), pointZ.get(), MOVE_OFFSET);
}

bool NpcMoveController::isNextRouteStepChosen() {
	Npc& npc = static_cast<Npc&>(owner);
	if (isStop_.get()) {
		WalkManager::stopWalking(*runtime::cast<ai::NpcAI>(npc.getAi()));
		return false;
	}
	if (walkerTemplate.get() == nullptr) {
		WalkManager::stopWalking(*runtime::cast<ai::NpcAI>(npc.getAi()));
		if (spawnengine::WalkerFormator::processClusteredNpc(npc, npc.getWorldId(), npc.getInstanceId()))
			return false;

		// Java HashMap.get(null) is null: a spawn without a walker id finds no template
		std::optional<std::string> spawnWalkerId = npc.getSpawn()->getWalkerId();
		setWalkerTemplate(spawnWalkerId ? dataholders::DataManager::WALKER_DATA->getWalkerTemplate(*spawnWalkerId) : nullptr, 0);
		if (walkerTemplate.get() == nullptr) {
			// Java: log.warn("Bad Walker Id: " + walkerId + " - point: " + currentStep.getStepIndex()), unreachable in Java too (setWalkerTemplate
			// throws for a null template)
			std::optional<std::string> walkerId = npc.getSpawn()->getWalkerId();
			log.warn("Bad Walker Id: {} - point: {}", walkerId.value_or("null"), detail::nonNull(currentStep.get(), "currentStep")->getStepIndex());
			return false;
		}
	}
	const auto& routeSteps = walkerTemplate.get()->getRouteSteps();
	const model::templates::walker::RouteStep* step = detail::nonNull(currentStep.get(), "currentStep");
	const model::templates::walker::RouteStep* nextStep =
		step->isLastStep() ? detail::listGet(routeSteps, 0).get() : detail::listGet(routeSteps, step->getStepIndex() + 1).get();
	setRouteStep(nextStep);
	return true;
}

bool NpcMoveController::isChangingDirection() {
	return detail::nonNull(currentStep.get(), "currentStep")->getStepIndex() == 0;
}

float NpcMoveController::getTargetX2() {
	return started->get() ? targetDestX.get() : owner.getX();
}

float NpcMoveController::getTargetY2() {
	return started->get() ? targetDestY.get() : owner.getY();
}

float NpcMoveController::getTargetZ2() {
	return started->get() ? targetDestZ.get() : owner.getZ();
}

void NpcMoveController::tryStoreStep(float x, float y, float z) {
	SYNCHRONIZED(*this) {
		Npc& npc = static_cast<Npc&>(owner);
		if (!lastSteps.get())
			lastSteps = runtime::RcLinkedList<Ref<model::geometry::Point3D>>::create(AION_LOCK_CLASS(NpcMoveController::lastSteps));
		Ptr<runtime::RcLinkedList<Ref<model::geometry::Point3D>>> steps = lastSteps.get();
		Ptr<model::geometry::Point3D> lastStep = steps->isEmpty() ? nullptr : steps->getLast();
		if (!lastStep || !PositionUtil::isInRange(lastStep->getX(), lastStep->getY(), lastStep->getZ(), x, y, z, 10)) {
			if (npc.getAi().isLogging()) {
				AILogger::moveinfo(npc, std::format("store back step: X={} Y={} Z={}", JavaFloat::toString(npc.getX()),
					JavaFloat::toString(npc.getY()), JavaFloat::toString(npc.getZ())));
			}
			steps->add(model::geometry::Point3D::create(x, y, z));
			if (steps->size() > 10)
				steps->removeFirst();
		}
	}
}

void NpcMoveController::returnToLastStepOrSpawn() {
	SYNCHRONIZED(*this) {
		Npc& npc = static_cast<Npc&>(owner);
		model::templates::spawns::SpawnTemplate& spawn = *npc.getSpawn();
		Ptr<runtime::RcLinkedList<Ref<model::geometry::Point3D>>> steps = lastSteps.get();
		Ptr<model::geometry::Point3D> step = !steps || steps->isEmpty() ? nullptr : steps->removeLast();
		if (step && !steps->isEmpty() && PositionUtil::isInRange(npc, step->getX(), step->getY(), step->getZ(), 2))
			step = steps->removeLast();
		if (!step ||
			world::geo::GeoService::getInstance().canSee(npc, spawn.getX(), spawn.getY(), spawn.getZ(), *geoEngine::collision::IgnoreProperties::ANY_RACE)) {
			targetDestX = spawn.getX();
			targetDestY = spawn.getY();
			targetDestZ = spawn.getZ();
			if (npc.getAi().isLogging())
				AILogger::moveinfo(npc, "recall back step: spawn point");
		} else {
			targetDestX = step->getX();
			targetDestY = step->getY();
			targetDestZ = step->getZ();
			if (npc.getAi().isLogging())
				AILogger::moveinfo(npc, std::format("recall back step: X={} Y={} Z={}", JavaFloat::toString(step->getX()),
					JavaFloat::toString(step->getY()), JavaFloat::toString(step->getZ())));
		}
		moveToPoint(targetDestX.get(), targetDestY.get(), targetDestZ.get());
	}
}

void NpcMoveController::clearBackSteps() {
	SYNCHRONIZED(*this) {
		lastSteps = nullptr;
		movementMask = MovementMask::IMMEDIATE;
	}
}

} // namespace aion::gameserver::controllers::movement
