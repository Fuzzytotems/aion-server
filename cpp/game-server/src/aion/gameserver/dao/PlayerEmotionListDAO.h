#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/emotion/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Mr. Poke
 */
class PlayerEmotionListDAO {
public:
	static void loadEmotions(model::gameobjects::player::Player& player);
	static void insertEmotion(model::gameobjects::player::Player& player, model::gameobjects::player::emotion::Emotion& emotion);
	static void deleteEmotion(int32_t playerId, int32_t emotionId);
};

} // namespace aion::gameserver::dao
