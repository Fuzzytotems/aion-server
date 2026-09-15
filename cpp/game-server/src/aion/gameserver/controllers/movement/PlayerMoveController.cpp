#include "aion/gameserver/controllers/movement/PlayerMoveController.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/FallDamageConfig.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::controllers::movement {

using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

PlayerMoveController::PlayerMoveController(model::gameobjects::player::Player& ownerValue) : PlayableMoveController(ownerValue) {
}

PlayerMoveController::~PlayerMoveController() = default;

void PlayerMoveController::abortMove() {
	PlayableMoveController::abortMove();
	stopFalling(owner.getZ());
}

void PlayerMoveController::resetLastPositionFromClient() {
	lastPositionFromClient.set(nullptr); // Java: lastPositionFromClient = null
}

void PlayerMoveController::onMoveFromClient() {
	Player& player = static_cast<Player&>(owner);
	updateLastMove();
	lastMovementMask = getMovementMask();
	lastPositionFromClientMillis = commons::utils::currentTimeMillis();
	runtime::Ptr<world::WorldPosition> lastPosition = lastPositionFromClient.get();
	if (!lastPosition || lastPosition->getMapId() != player.getWorldId())
		lastPositionFromClient.set(world::WorldPosition::create(player.getWorldId(), player.getX(), player.getY(), player.getZ(), player.getHeading()));
	else
		lastPosition->setXYZH(player.getX(), player.getY(), player.getZ(), player.getHeading());
}

void PlayerMoveController::resetToLastPositionFromClient() {
	Player& player = static_cast<Player&>(owner);
	abortMove();
	runtime::Ptr<world::WorldPosition> lastPosition = lastPositionFromClient.get();
	if (lastPosition && player.getWorldId() == lastPosition->getMapId())
		player.getPosition()->setXYZH(lastPosition->getX(), lastPosition->getY(), lastPosition->getZ(), lastPosition->getHeading());
}

void PlayerMoveController::updateFalling(float newZ) {
	Player& player = static_cast<Player&>(owner);
	if (lastFallZ.get() != 0) {
		fallDistance = fallDistance.get() + (lastFallZ.get() - newZ);
		if (fallDistance.get() >= static_cast<float>(configs::main::FallDamageConfig::MAXIMUM_DISTANCE_MIDAIR.load()) &&
			player.getController().die(SM_ATTACK_STATUS_TYPE::FALL_DAMAGE, SM_ATTACK_STATUS_LOG::REGULAR, player)) {
			services::player::PlayerReviveService::scheduleReviveAtBase(player, 1000, 0);
			return;
		}
	}
	lastFallZ = newZ;
	player.getObserveController()->notifyMoveObservers();
}

void PlayerMoveController::stopFalling(float newZ) {
	Player& player = static_cast<Player&>(owner);
	if (lastFallZ.get() == 0)
		return;

	if (!player.isFlying() && !player.isDead()) {
		fallDistance = fallDistance.get() + (lastFallZ.get() - newZ);
		int32_t damage = standins::statFunctionsCalculateFallDamage(player, fallDistance.get());
		if (damage > 0) {
			player.getLifeStats()->reduceHp(SM_ATTACK_STATUS_TYPE::FALL_DAMAGE, damage, 0, SM_ATTACK_STATUS_LOG::REGULAR, player);
			player.getObserveController()->notifyAttackedObservers(player, 0);
		}
	}
	fallDistance = 0;
	lastFallZ = 0;
	player.getObserveController()->notifyMoveObservers();
}

void PlayerMoveController::setHasMovedByRandomMoveLocEffect(skillengine::model::Skill& skill) {
	// delayMillis is required because instant skills (like Power: Emergency Teleport I) are not scheduled with hitTime in endCast
	int32_t delayMillis = skill.isInstantSkill() ? skill.getHitTime() : 0;
	this->lastRandomMoveLocEffectTimeMillis = commons::utils::currentTimeMillis() + delayMillis;
}

bool PlayerMoveController::hasMovedByRandomMoveLocEffect() {
	return lastRandomMoveLocEffectTimeMillis.get() != 0 && commons::utils::currentTimeMillis() - lastRandomMoveLocEffectTimeMillis.get() < 300;
}

} // namespace aion::gameserver::controllers::movement
