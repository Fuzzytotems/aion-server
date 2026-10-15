#include "aion/gameserver/network/aion/clientpackets/CM_EMOTION.h"

#include <cstdint>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/EmotionTypeInfo.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/ride/RideInfo.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** Logger */
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_EMOTION");

namespace {

using model::gameobjects::player::Player;

/** Java `player.ride.getStartFp()` / `player.ride.canSprint()`: a null ride throws NullPointerException there, where a C++ deref would be UB */
const model::templates::ride::RideInfo& rideOf(Player& player) {
	const model::templates::ride::RideInfo* ride = player.ride.get();
	if (ride == nullptr)
		throw runtime::NullPointerException("player.ride is null");
	return *ride;
}

} // namespace

using model::EmotionType;
using model::gameobjects::state::CreatureState;
using serverpackets::SM_SYSTEM_MESSAGE;
using skillengine::effect::AbnormalState;

CM_EMOTION::CM_EMOTION(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

/** Read data */
void CM_EMOTION::readImpl() {
	int32_t et = readUC();
	emotionType = model::getEmotionTypeById(et);

	switch (emotionType) {
		case EmotionType::SELECT_TARGET: // select target
		case EmotionType::JUMP: // jump
		case EmotionType::SIT: // resting
		case EmotionType::STAND: // end resting
		case EmotionType::LAND_FLYTELEPORT: // fly teleport land
		case EmotionType::FLY: // fly up
		case EmotionType::LAND: // land
		case EmotionType::DIE: // die
		case EmotionType::EMOTE_END: // duel end
		case EmotionType::WALK: // walk on
		case EmotionType::RUN: // walk off
		case EmotionType::OPEN_DOOR: // open static doors
		case EmotionType::CLOSE_DOOR: // close static doors
		case EmotionType::POWERSHARD_ON: // powershard on
		case EmotionType::POWERSHARD_OFF: // powershard off
		case EmotionType::ATTACKMODE_IN_MOVE: // get equip weapon
		case EmotionType::ATTACKMODE_IN_STANDING: // get equip weapon
		case EmotionType::NEUTRALMODE_IN_MOVE: // remove equip weapon
		case EmotionType::NEUTRALMODE_IN_STANDING: // remove equip weapon
		case EmotionType::END_SPRINT:
			break;
		case EmotionType::WINDSTREAM_STRAFE:
			readC(); // unk 2
			break;
		case EmotionType::START_SPRINT:
			readD(); // unk 1
			break;
		case EmotionType::EMOTE:
			emotion = readUH();
			targetObjectId = readD();
			break;
		case EmotionType::CHAIR_SIT: // sit on chair
		case EmotionType::CHAIR_UP: // stand on chair
			x = readF();
			y = readF();
			z = readF();
			heading = readC();
			break;
		default:
			// Java: "Unknown emotion type? 0x" + Integer.toHexString(et).toUpperCase()
			log.error("Unknown emotion type? 0x{:X}", et);
			break;
	}
}

void CM_EMOTION::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isDead()) {
		return;
	}

	if (emotionType != EmotionType::SELECT_TARGET && emotionType != EmotionType::ATTACKMODE_IN_MOVE
		&& emotionType != EmotionType::ATTACKMODE_IN_STANDING && emotionType != EmotionType::NEUTRALMODE_IN_MOVE
		&& emotionType != EmotionType::NEUTRALMODE_IN_STANDING) {
		if (player->getEffectController()->isInAnyAbnormalState(AbnormalState::CANT_MOVE_STATE) || player->getEffectController()->isUnderFear()
			|| player->getEffectController()->isConfused()) {
			return;
		}
	}

	if (player->isInState(CreatureState::PRIVATE_SHOP)
		|| player->isInAttackMode() && (emotionType == EmotionType::CHAIR_SIT || emotionType == EmotionType::JUMP))
		return;

	if (emotionType == EmotionType::SELECT_TARGET) {
		if (configs::main::CustomConfig::CANCEL_ITEM_USE_ON_TARGET_CHANGE.load()) {
			player->getController().cancelUseItem();
			if (player->isCastingItemSkill()) // selecting a target interrupts item casts, but not skill casts
				player->getController().cancelCurrentSkill(nullptr);
		}
		return;
	}

	player->getController().cancelUseItem();
	player->getController().cancelCurrentSkill(nullptr);

	// check for stance
	if (player->getController().isUnderStance()) {
		switch (emotionType) {
			case EmotionType::FLY:
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_TAKE_OFF_WHILE_IN_CURRENT_STANCE_());
				return;
			default:
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CHANGE_MODE_WHILE_IN_CURRENT_STANCE_());
				return;
		}
	}

	switch (emotionType) {
		case EmotionType::SIT:
			if (player->isInState(CreatureState::PRIVATE_SHOP)) {
				return;
			}
			player->getObserveController()->notifySitObservers();
			if (player->isInPlayerMode(model::actions::PlayerMode::RIDE)) {
				player->unsetPlayerMode(model::actions::PlayerMode::RIDE);
			}
			player->setState(CreatureState::RESTING);
			break;
		case EmotionType::STAND:
			player->unsetState(CreatureState::RESTING);
			break;
		case EmotionType::CHAIR_SIT:
			player->setState(CreatureState::CHAIR, true);
			break;
		case EmotionType::CHAIR_UP:
			if (player->isInState(CreatureState::CHAIR))
				player->setState(CreatureState::ACTIVE, true);
			break;
		case EmotionType::LAND_FLYTELEPORT:
			player->getController().onFlyTeleportEnd();
			break;
		case EmotionType::FLY:
			if (!player->getFlyController().startFly(false, false))
				return;
			break;
		case EmotionType::LAND:
			player->getFlyController().endFly(false);
			break;
		case EmotionType::ATTACKMODE_IN_MOVE:
		case EmotionType::ATTACKMODE_IN_STANDING:
			player->setState(CreatureState::WEAPON_EQUIPPED);
			break;
		case EmotionType::NEUTRALMODE_IN_MOVE:
		case EmotionType::NEUTRALMODE_IN_STANDING:
			player->unsetState(CreatureState::WEAPON_EQUIPPED);
			break;
		case EmotionType::WALK:
			if (player->isFlying()) // cannot toggle walk when flying or gliding
				return;
			player->setState(CreatureState::WALK_MODE);
			break;
		case EmotionType::RUN:
			player->unsetState(CreatureState::WALK_MODE);
			break;
		case EmotionType::OPEN_DOOR:
		case EmotionType::CLOSE_DOOR:
			break;
		case EmotionType::POWERSHARD_ON:
			if (!player->getEquipment().isPowerShardEquipped()) {
				utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_WEAPON_BOOST_NO_BOOSTER_EQUIPED());
				return;
			}
			utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_WEAPON_BOOST_BOOST_MODE_STARTED());
			player->setState(CreatureState::POWERSHARD);
			break;
		case EmotionType::POWERSHARD_OFF:
			utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_WEAPON_BOOST_BOOST_MODE_ENDED());
			player->unsetState(CreatureState::POWERSHARD);
			break;
		case EmotionType::START_SPRINT:
			if (!player->isInPlayerMode(model::actions::PlayerMode::RIDE) || player->getLifeStats()->getCurrentFp() < rideOf(*player).getStartFp()
				|| player->isFlying() || !rideOf(*player).canSprint()) {
				return;
			}
			player->setSprintMode(true);
			player->getLifeStats()->triggerFpReduce();
			break;
		case EmotionType::END_SPRINT:
			if (!player->isInPlayerMode(model::actions::PlayerMode::RIDE) || !rideOf(*player).canSprint() || !player->isInSprintMode()) {
				return;
			}
			player->setSprintMode(false);
			player->getLifeStats()->triggerFpRestore();
			break;
		default:
			break;
	}

	if (player->getEmotions()->canUse(emotion)) {
		utils::PacketSendUtility::broadcastToSightedPlayers(*player,
			serverpackets::SM_EMOTION(*player, emotionType, emotion, x, y, z, heading, getTargetObjectId(*player)), true);
	}

	if (player->isProtectionActive())
		player->getController().stopProtectionActiveTask();
}

int32_t CM_EMOTION::getTargetObjectId(Player& player) {
	runtime::Ptr<model::gameobjects::VisibleObject> target = player.getTarget();
	return !target ? targetObjectId : target->getObjectId();
}

AION_CLIENT_PACKET(CM_EMOTION);

} // namespace aion::gameserver::network::aion::clientpackets
