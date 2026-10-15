#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_EMOTION.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/EmotionTypeInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_SUMMON_EMOTION");

using model::EmotionType;
using serverpackets::SM_EMOTION;
using utils::PacketSendUtility;

CM_SUMMON_EMOTION::CM_SUMMON_EMOTION(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SUMMON_EMOTION.java:32-36
void CM_SUMMON_EMOTION::readImpl() {
	objId = readD();
	emotionTypeId = readUC();
}

// Java CM_SUMMON_EMOTION.java:38-70
void CM_SUMMON_EMOTION::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	const runtime::Ptr<model::gameobjects::Creature> summonOrMercenary = player->getSummonOrMercenary(objId);
	if (!summonOrMercenary) // commonly due to lags when the pet dies
		return;

	const EmotionType emotionType = model::getEmotionTypeById(emotionTypeId);
	switch (emotionType) {
		case EmotionType::FLY:
		case EmotionType::LAND:
			PacketSendUtility::broadcastPacket(*summonOrMercenary, SM_EMOTION(*summonOrMercenary, EmotionType::CHANGE_SPEED));
			PacketSendUtility::broadcastPacket(*summonOrMercenary, SM_EMOTION(*summonOrMercenary, emotionType));
			break;
		case EmotionType::JUMP:
		case EmotionType::SUMMON_STOP_JUMP:
			PacketSendUtility::broadcastPacket(*summonOrMercenary, SM_EMOTION(*summonOrMercenary, emotionType));
			break;
		case EmotionType::ATTACKMODE_IN_MOVE: // start attacking
			summonOrMercenary->setState(model::gameobjects::state::CreatureState::WEAPON_EQUIPPED);
			PacketSendUtility::broadcastPacket(*summonOrMercenary, SM_EMOTION(*summonOrMercenary, emotionType));
			break;
		case EmotionType::NEUTRALMODE_IN_MOVE: // stop attacking
			summonOrMercenary->unsetState(model::gameobjects::state::CreatureState::WEAPON_EQUIPPED);
			PacketSendUtility::broadcastPacket(*summonOrMercenary, SM_EMOTION(*summonOrMercenary, emotionType));
			break;
		case EmotionType::NONE:
			if (emotionTypeId != model::getTypeId(EmotionType::NONE))
				log.warn("Unknown emotion type " + std::to_string(emotionTypeId) + " from " + player->toString());
			break;
		default:
			break;
	}
}

AION_CLIENT_PACKET(CM_SUMMON_EMOTION);

} // namespace aion::gameserver::network::aion::clientpackets
