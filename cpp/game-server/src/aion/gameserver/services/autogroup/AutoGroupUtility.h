#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/autogroup/fwd.h"

namespace aion::gameserver::services::autogroup {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author Estrayl
 */
class AutoGroupUtility {
public:
	static bool canRegisterNewEntry(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt);
	static bool canRegisterQuickEntry(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt);
	static bool canRegisterGroupEntry(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt, int32_t mapId, int32_t maskId);
	static bool checkGroupRequirements(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt, int32_t mapId, int32_t maskId);
	static void sendSuccessfulRegistration(model::autogroup::LookingForParty& lfp, std::string_view leaderName, model::autogroup::AutoGroupType agt, int32_t maskId);
	static void sendWindowToPlayerIfOnline(int32_t objectId, int32_t maskId, int32_t windowId);
	static void sendWindowToPlayer(model::gameobjects::player::Player& player, int32_t maskId, int32_t windowId);
	static bool hasCoolDown(model::gameobjects::player::Player& player, int32_t worldId);
};

} // namespace aion::gameserver::services::autogroup
