#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

#include "aion/gameserver/model/EmotionTypeInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EMOTION::SM_EMOTION(model::gameobjects::Creature& creature, model::EmotionType emotionTypeValue) : SM_EMOTION(creature, emotionTypeValue, 0, 0) {
}

SM_EMOTION::SM_EMOTION(model::gameobjects::Creature& creature, model::EmotionType emotionTypeValue, int32_t emotionValue, int32_t targetObjectIdValue)
	: AionServerPacket(opcodeOf<SM_EMOTION>) {
	this->senderObjectId = creature.getObjectId();
	this->emotionType = emotionTypeValue;
	this->emotion = emotionValue;
	this->targetObjectId = targetObjectIdValue;
	this->state = creature.getState();
	std::unique_ptr<model::stats::calc::Stat2> aSpeed = creature.getGameStats()->getAttackSpeed();
	this->baseAttackSpeed = aSpeed->getBase();
	this->currentAttackSpeed = aSpeed->getCurrent();
	this->speed = creature.getGameStats()->getMovementSpeedFloat();
}

SM_EMOTION::SM_EMOTION(int32_t Objid, model::EmotionType emotionTypeValue, int32_t stateValue)
	: AionServerPacket(opcodeOf<SM_EMOTION>), senderObjectId(Objid), emotionType(emotionTypeValue), state(stateValue) {
}

SM_EMOTION::SM_EMOTION(model::gameobjects::player::Player& player, model::EmotionType emotionTypeValue, int32_t emotionValue, float xValue,
	float yValue, float zValue, int8_t headingValue, int32_t targetObjectIdValue)
	: AionServerPacket(opcodeOf<SM_EMOTION>) {
	this->senderObjectId = player.getObjectId();
	this->emotionType = emotionTypeValue;
	this->emotion = emotionValue;
	this->x = xValue;
	this->y = yValue;
	this->z = zValue;
	this->heading = headingValue;
	this->targetObjectId = targetObjectIdValue;
	this->state = player.getState();
	this->speed = player.getGameStats()->getMovementSpeedFloat();
	std::unique_ptr<model::stats::calc::Stat2> aSpeed = player.getGameStats()->getAttackSpeed();
	this->baseAttackSpeed = aSpeed->getBase();
	this->currentAttackSpeed = aSpeed->getCurrent();
}

void SM_EMOTION::writeImpl(AionConnection* con) {
	using model::EmotionType;
	writeD(senderObjectId);
	writeC(model::getTypeId(emotionType));
	writeH(state);
	writeF(speed);
	switch (emotionType) {
		case EmotionType::LAND_FLYTELEPORT: // fly teleport (land)
		case EmotionType::FLY: // toggle flight mode
		case EmotionType::LAND: // toggle land mode
		case EmotionType::SELECT_TARGET: // select target
		case EmotionType::JUMP:
		case EmotionType::SIT: // sit
		case EmotionType::STAND: // stand
		case EmotionType::ATTACKMODE_IN_MOVE: // toggle attack mode
		case EmotionType::NEUTRALMODE_IN_MOVE: // toggle normal mode
		case EmotionType::WALK: // toggle walk
		case EmotionType::RUN: // toggle run
		case EmotionType::OPEN_PRIVATESHOP: // private shop open
		case EmotionType::CLOSE_PRIVATESHOP: // private shop close
		case EmotionType::POWERSHARD_ON: // powershard on
		case EmotionType::POWERSHARD_OFF: // powershard off
		case EmotionType::ATTACKMODE_IN_STANDING: // toggle attack mode
		case EmotionType::NEUTRALMODE_IN_STANDING: // toggle normal mode
		case EmotionType::START_FEEDING:
		case EmotionType::END_FEEDING:
		case EmotionType::WINDSTREAM_START_BOOST:
		case EmotionType::WINDSTREAM_END_BOOST:
		case EmotionType::WINDSTREAM_END:
		case EmotionType::WINDSTREAM_EXIT:
		case EmotionType::OPEN_DOOR:
		case EmotionType::CLOSE_DOOR:
		case EmotionType::WINDSTREAM_STRAFE:
		case EmotionType::STOP_GLIDE:
		case EmotionType::STOP_FLY:
			break;
		case EmotionType::DIE: // die
		case EmotionType::START_LOOT: // looting start
		case EmotionType::END_LOOT: // looting end
		case EmotionType::START_QUESTLOOT: // looting start (quest)
		case EmotionType::END_QUESTLOOT: // looting end (quest);
			writeD(targetObjectId);
			break;
		case EmotionType::CHAIR_SIT: // sit (chair)
		case EmotionType::CHAIR_UP: // stand (chair)
			writeF(x);
			writeF(y);
			writeF(z);
			writeC(heading);
			break;
		case EmotionType::START_FLYTELEPORT:
			// fly teleport (start)
			writeD(emotion); // teleport Id
			break;
		case EmotionType::WINDSTREAM:
			// entering windstream
			writeD(emotion); // teleport Id
			writeD(targetObjectId); // distance
			break;
		case EmotionType::RIDE:
		case EmotionType::RIDE_END:
			if (targetObjectId != 0) {
				writeD(targetObjectId); // rideId
			}
			writeF(0x3F); // unk
			writeF(0x3F); // unk
			writeF(0x40); // unk
			break;
		case EmotionType::RESURRECT:
			// resurrect
			writeD(0);
			break;
		case EmotionType::EMOTE:
			// emote
			writeD(targetObjectId);
			writeH(emotion);
			writeC(1);
			break;
		case EmotionType::CHANGE_SPEED:
			// emote startloop
			writeH(baseAttackSpeed);
			writeH(currentAttackSpeed);
			writeC(0); // new 4.0
			break;
		default:
			if (targetObjectId != 0) {
				writeD(targetObjectId);
			}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
