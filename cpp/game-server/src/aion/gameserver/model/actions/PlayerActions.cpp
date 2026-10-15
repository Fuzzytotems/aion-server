#include "aion/gameserver/model/actions/PlayerActions.h"

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/player/InRoll.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::actions {

using gameobjects::player::Player;

bool PlayerActions::isInPlayerMode(Player& player, PlayerMode mode) {
	switch (mode) {
		case PlayerMode::RIDE:
			return player.ride.get() != nullptr;
		case PlayerMode::IN_ROLL:
			return static_cast<bool>(player.inRoll.get());
	}
	return false;
}

void PlayerActions::setPlayerMode(Player& player, PlayerMode mode, const std::any& obj) {
	switch (mode) {
		case PlayerMode::RIDE:
			player.ride = obj.has_value() ? std::any_cast<const templates::ride::RideInfo*>(obj) : nullptr;
			break;
		case PlayerMode::IN_ROLL:
			player.inRoll.set(
				obj.has_value() ? std::any_cast<runtime::Ref<gameobjects::player::InRoll>>(obj) : runtime::Ref<gameobjects::player::InRoll>());
			break;
	}
}

bool PlayerActions::unsetPlayerMode(Player& player, PlayerMode mode) {
	switch (mode) {
		case PlayerMode::RIDE: {
			if (player.ride.get() == nullptr)
				return false;
			player.ride = nullptr;
			// check for sprinting when forcefully dismounting player
			if (player.isInSprintMode()) {
				if (!player.isInFlyingState()) // if player is flying while dismounting, do not start restore task
					player.getLifeStats()->triggerFpRestore();
				player.setSprintMode(false);
			}
			player.unsetState(gameobjects::state::CreatureState::RESTING);
			player.unsetState(gameobjects::state::CreatureState::FLOATING_CORPSE);
			player.setState(gameobjects::state::CreatureState::ACTIVE);
			utils::PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_EMOTION(player, EmotionType::CHANGE_SPEED, 0, 0),
				true);
			utils::PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_EMOTION(player, EmotionType::RIDE_END), true);
			player.getGameStats()->updateStatsAndSpeedVisually();
			// remove rideObservers
			runtime::Ptr<runtime::RcArrayList<runtime::Ref<controllers::observer::ActionObserver>>> rideObservers = player.getRideObservers();
			SYNCHRONIZED(*rideObservers) {
				for (runtime::Ptr<controllers::observer::ActionObserver> observer : *rideObservers)
					player.getObserveController()->removeObserver(*observer);
				rideObservers->clear();
			}
			return true;
		}
		case PlayerMode::IN_ROLL:
			if (!player.inRoll.get())
				return false;
			player.inRoll.set(runtime::Ref<gameobjects::player::InRoll>());
			return true;
		default:
			return false;
	}
}

} // namespace aion::gameserver::model::actions
