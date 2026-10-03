#include "aion/gameserver/network/aion/clientpackets/CM_WINDSTREAM.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WINDSTREAM.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_WINDSTREAM");

using model::EmotionType;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureState;
using model::gameobjects::state::FlyState;
using model::templates::flypath::FlightPath;
using serverpackets::SM_EMOTION;
using utils::PacketSendUtility;

CM_WINDSTREAM::CM_WINDSTREAM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_WINDSTREAM.java:33-37
void CM_WINDSTREAM::readImpl() {
	teleportId = readD();
	distance = readD();
	state = readD();
}

// Java CM_WINDSTREAM.java:40-91
void CM_WINDSTREAM::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	switch (state) {
		case 0: // entering windstream
			if (player->isUsingFlightTransporterOrWindstream())
				return;
			player->unsetPlayerMode(model::actions::PlayerMode::RIDE);
			player->setFlightPath(FlightPath::create(FlightPath::Type::WINDSTREAM, teleportId, distance));
			player->unsetState(CreatureState::ACTIVE);
			player->unsetState(CreatureState::GLIDING);
			player->setState(CreatureState::FLYING);
			player->unsetFlyState(FlyState::GLIDING);
			player->setFlyState(FlyState::FLYING);
			player->getLifeStats()->triggerFpRestore();
			break;
		case 1: // after entering windstream
			if (player->isUsingFlightPath(FlightPath::Type::WINDSTREAM)) {
				PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, EmotionType::WINDSTREAM, teleportId, distance), true);
				runtime::Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, *player, 0);
				questEngine::QuestEngine::getInstance().onEnterWindStream(*env, teleportId);
			}
			return; // don't send SM_WINDSTREAM
		case 2: // leaving windstream (gliding)
		case 3: // leaving windstream
			if (!player->isUsingFlightPath(FlightPath::Type::WINDSTREAM))
				return;
			player->unsetState(CreatureState::FLYING);
			player->setState(CreatureState::ACTIVE);
			player->unsetFlyState(FlyState::FLYING);
			player->unsetFlyState(FlyState::GLIDING);
			if (state == 2)
				player->getFlyController().switchToGliding();
			else
				player->getGameStats()->updateStatsAndSpeedVisually();
			player->setFlightPath(nullptr);
			PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, state == 2 ? EmotionType::WINDSTREAM_END : EmotionType::WINDSTREAM_EXIT), true);
			if (player->isTransformed()) // send sm_transform if player is transformed
				PacketSendUtility::broadcastPacketAndReceive(*player, serverpackets::SM_TRANSFORM(*player));
			break;
		case 4: // ?
			break;
		case 7: // start boost
		case 8: // end boost
			PacketSendUtility::broadcastPacket(*player,
				SM_EMOTION(*player, state == 7 ? EmotionType::WINDSTREAM_START_BOOST : EmotionType::WINDSTREAM_END_BOOST), true);
			break;
		default:
			log.warn("Unknown Windstream state #" + std::to_string(state) + " was sent from " + player->getPosition()->toString());
			return;
	}
	PacketSendUtility::sendPacket(*player, serverpackets::SM_WINDSTREAM(state, 1));
}

AION_CLIENT_PACKET(CM_WINDSTREAM);

} // namespace aion::gameserver::network::aion::clientpackets
