#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Emotion packet
 *
 * @author SoulKeeper, -Enomine-
 */
class SM_EMOTION : public AionServerPacket {
private:
	int32_t senderObjectId{};
	model::EmotionType emotionType{};
	int32_t emotion{};
	int32_t targetObjectId{};
	float speed{};
	int32_t state{};
	int32_t baseAttackSpeed{};
	int32_t currentAttackSpeed{};
	float x{};
	float y{};
	float z{};
	int8_t heading{};

public:
	SM_EMOTION(model::gameobjects::Creature& creature, model::EmotionType emotionType);
	SM_EMOTION(model::gameobjects::Creature& creature, model::EmotionType emotionType, int32_t emotion, int32_t targetObjectId);
	SM_EMOTION(int32_t Objid, model::EmotionType emotionType, int32_t state);
	SM_EMOTION(model::gameobjects::player::Player& player, model::EmotionType emotionType, int32_t emotion, float x, float y, float z, int8_t heading,
		int32_t targetObjectId);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
