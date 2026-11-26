#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Rolandas, Neon, Sykra
 */
class HouseScriptsDAO {
public:
	static void storeScript(int32_t houseId, int32_t scriptId, runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize);
	static runtime::Ref<model::gameobjects::player::PlayerScripts> getPlayerScripts(int32_t houseId);
	static void deleteScript(int32_t houseId, int32_t scriptId);
	static void deleteScriptsForHouse(int32_t houseId);
};

} // namespace aion::gameserver::dao
