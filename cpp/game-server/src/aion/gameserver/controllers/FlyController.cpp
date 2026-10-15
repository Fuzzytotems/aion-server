#include "aion/gameserver/controllers/FlyController.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::controllers {

using model::EmotionType;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureState;
using model::gameobjects::state::FlyState;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

FlyController::FlyController(model::gameobjects::player::Player& playerValue) : OwnedPart(playerValue), player(playerValue) {
}

FlyController::~FlyController() = default;

void FlyController::onStopGliding() {
	if (player.isInGlidingState()) {
		player.unsetFlyState(FlyState::GLIDING);
		player.unsetState(CreatureState::GLIDING);
		if (!player.isInFlyState(FlyState::FLYING)) {
			player.getLifeStats()->triggerFpRestore();
			PacketSendUtility::broadcastToSightedPlayers(player, SM_EMOTION(player, EmotionType::STOP_GLIDE), true);
		} else {
			player.getLifeStats()->triggerFpReduce();
		}
		player.getGameStats()->updateStatsAndSpeedVisually();
	}
}

void FlyController::endFly(bool broadcastPacket) {
	player.unsetFlyState(FlyState::FLYING);
	player.unsetFlyState(FlyState::GLIDING);
	player.unsetState(CreatureState::FLYING);
	player.unsetState(CreatureState::GLIDING);
	player.unsetState(CreatureState::FLOATING_CORPSE);
	player.getGameStats()->updateStatsAndSpeedVisually();

	if (broadcastPacket && player.isSpawned())
		PacketSendUtility::broadcastToSightedPlayers(player, SM_EMOTION(player, EmotionType::LAND), true);
	player.getLifeStats()->triggerFpRestore();
}

bool FlyController::startFly(bool broadcastPacket, bool ignoreFlightCooldown) {
	if (!canFly(player))
		return false;
	if (!ignoreFlightCooldown) {
		if (player.getFlyReuseTime() > commons::utils::currentTimeMillis()) {
			utils::audit::AuditLogger::log(player, "possibly using fly cooldown hack. Left cooldown time: " +
				std::to_string((player.getFlyReuseTime() - commons::utils::currentTimeMillis()) / 1000) + "s");
			return false;
		}
		player.setFlyReuseTime(commons::utils::currentTimeMillis() + FLY_REUSE_TIME - 100);
	}
	player.setFlyState(FlyState::FLYING);
	player.setState(CreatureState::FLYING);
	if (player.isInPlayerMode(model::actions::PlayerMode::RIDE)) {
		player.setState(CreatureState::FLOATING_CORPSE);
	}
	player.getLifeStats()->triggerFpReduce();
	player.getGameStats()->updateStatsAndSpeedVisually();

	if (broadcastPacket)
		PacketSendUtility::broadcastToSightedPlayers(player, SM_EMOTION(player, EmotionType::FLY), true);
	return true;
}

bool FlyController::canFly(model::gameobjects::player::Player& value) {
	if (!value.getCommonData()->isDaeva()) {
		PacketSendUtility::sendPacket(value, SM_SYSTEM_MESSAGE::STR_GLIDE_ONLY_DEVA_CAN());
		return false;
	}
	if (!value.hasAccess(configs::administration::AdminConfig::FREE_FLIGHT) &&
		(value.isInsideZoneType(model::templates::zone::ZoneType::NO_FLY) || !value.isInsideZoneType(model::templates::zone::ZoneType::FLY))) {
		PacketSendUtility::sendPacket(value, SM_SYSTEM_MESSAGE::STR_FLYING_FORBIDDEN_HERE());
		return false;
	}
	if (value.getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::NOFLY)) {
		PacketSendUtility::sendPacket(value, SM_SYSTEM_MESSAGE::STR_CANT_FLY_NOW_DUE_TO_NOFLY());
		return false;
	}
	if (value.getTransformModel().cantFly()) {
		PacketSendUtility::sendPacket(value, SM_SYSTEM_MESSAGE::STR_FLY_CANNOT_FLY_POLYMORPH_STATUS());
		return false;
	}
	return !value.getStore();
}

bool FlyController::switchToGliding() {
	if (player.isInGlidingState() || !player.canPerformMove())
		return false;
	if (player.isUsingFlightTransporterOrWindstream())
		return false;
	if (!canGlide(player))
		return false;
	if (player.getFlyState() == 0) {
		// fly reuse time only if gliding from walking
		if (player.getFlyReuseTime() > commons::utils::currentTimeMillis()) {
			return false;
		}
		player.setFlyReuseTime(commons::utils::currentTimeMillis() + FLY_REUSE_TIME);
	}
	player.setFlyState(FlyState::GLIDING);
	player.setState(CreatureState::GLIDING);
	player.getLifeStats()->triggerFpReduce();
	player.getGameStats()->updateStatsAndSpeedVisually();
	return true;
}

bool FlyController::canGlide(model::gameobjects::player::Player& value) {
	if (!value.getCommonData()->isDaeva()) {
		PacketSendUtility::sendPacket(value, SM_SYSTEM_MESSAGE::STR_GLIDE_ONLY_DEVA_CAN());
		return false;
	}
	if (value.getTransformModel().cantFly()) {
		PacketSendUtility::sendPacket(value, SM_SYSTEM_MESSAGE::STR_GLIDE_CANNOT_GLIDE_POLYMORPH_STATUS());
		return false;
	}
	return true;
}

} // namespace aion::gameserver::controllers
