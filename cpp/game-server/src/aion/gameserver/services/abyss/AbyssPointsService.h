#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/abyss/fwd.h"

namespace aion::gameserver::services::abyss {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class AbyssPointsService {
public:
	static void addAp(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& obj, int32_t value);
	static void addAp(model::gameobjects::player::Player& player, int32_t amount);
	/** player is nullable (the Java body checks null); gainMessage builds the gain message from the added AP */
	static void addAp(runtime::Ptr<model::gameobjects::player::Player> player, int32_t amount, const std::function<network::aion::serverpackets::SM_SYSTEM_MESSAGE(int32_t)>& gainMessage);
	static void onRankChanged(model::gameobjects::player::Player& player, bool abyssPointChanged, bool abyssRankChanged, std::optional<int32_t> newRankingListPosition);
};

} // namespace aion::gameserver::services::abyss
