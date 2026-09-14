#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

/**
 * @author Source, Neon
 */
class PlayerChatService {
public:
	static bool isFlooding(model::gameobjects::player::Player& player);
	static void logWhisper(model::gameobjects::player::Player& sender, model::gameobjects::player::Player& receiver, std::string_view message);
	static void logMessage(model::gameobjects::player::Player& sender, model::ChatType type, std::string_view message);
private:
	static void logMessage(model::gameobjects::player::Player& sender, model::ChatType type, std::string_view message,
		runtime::Ptr<model::gameobjects::player::Player> receiver);
};

} // namespace aion::gameserver::services::player
