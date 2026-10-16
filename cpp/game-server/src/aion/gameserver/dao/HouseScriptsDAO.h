#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Rolandas, Neon, Sykra
 */
class HouseScriptsDAO {
public:
	static void storeScript(int32_t houseId, int32_t scriptId, std::string_view scriptXML);
	static runtime::Ref<model::gameobjects::player::PlayerScripts> getPlayerScripts(int32_t houseId);
	static void deleteScript(int32_t houseId, int32_t scriptId);
	static void deleteScriptsForHouse(int32_t houseId);
private:
	static bool addScript(model::gameobjects::player::PlayerScripts& scripts, int32_t id, std::string_view scriptXML);
};

} // namespace aion::gameserver::dao
