#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.CompositionAction: combines two enchantment stones into one via
 * CM_COMPOSITE_STONES. No AbstractItemAction and not XML-bound (the client alone knows to send the packet for the combination tool).
 * C++: stateless, the const methods are thread-safe.
 */
class CompositionAction {
public:
	bool canAct(gameobjects::player::Player& player, gameobjects::Item& tools, gameobjects::Item& first, gameobjects::Item& second) const;

	void act(gameobjects::player::Player& player, gameobjects::Item& tools, gameobjects::Item& first, gameobjects::Item& second) const;

	int32_t getItemId(int32_t value) const;

private:
	int32_t calcLevel(int32_t first, int32_t second) const;
};

} // namespace aion::gameserver::model::templates::item::actions
