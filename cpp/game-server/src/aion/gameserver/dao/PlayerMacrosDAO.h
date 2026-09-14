#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * Created on: 13.07.2009 17:05:56
 *
 * @author Aquanox
 */
class PlayerMacrosDAO {
public:
	static void addMacro(int32_t playerId, int32_t macroPosition, std::string_view macro);
	static void updateMacro(int32_t playerId, int32_t macroPosition, std::string_view macro);
	static void deleteMacro(int32_t playerId, int32_t macroPosition);
	static runtime::Ref<model::gameobjects::player::Macros> loadMacros(int32_t playerId);
};

} // namespace aion::gameserver::dao
