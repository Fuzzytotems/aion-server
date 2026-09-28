#pragma once

#include <cstdint>

#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author SoulKeeper, nerolory
 */
class CM_EMOTION : public AionClientPacket {
private:
	/** Emotion number */
	model::EmotionType emotionType{};
	/** Emotion number */
	int32_t emotion{};
	/** Coordinates of player */
	float x{}, y{}, z{};
	int8_t heading{};

	int32_t targetObjectId{};

public:
	CM_EMOTION(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t getTargetObjectId(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::network::aion::clientpackets
