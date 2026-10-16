#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/enchants/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"

namespace aion::gameserver::model::enchants {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K3, `Item.enchantEffect`), created with create(). StatOwner is
 * held by Ref, so retain()/release() forward to RefCounted (§9.2). The constructor builds the stat functions and adds them to the player's
 * game stats; itemSlot is MAIN_HAND for an effect without attack stats (Java null, docs/deviations/P4-13.md). The enchant stats are static data read during the call (`const EnchantStat*` elements).
 *
 * @author xTz
 */
class EnchantEffect : public runtime::RefCounted, public stats::calc::StatOwner {
	AION_MAKE_REF_FRIEND
private:
	const items::ItemSlot itemSlot;

protected:
	EnchantEffect(gameobjects::Item& item, gameobjects::player::Player& player, const std::vector<const EnchantStat*>& enchantStats);
	~EnchantEffect() override;

public:
	/** Java: new EnchantEffect(item, player, enchantStats) */
	static runtime::Ref<EnchantEffect> create(gameobjects::Item& item, gameobjects::player::Player& player,
		const std::vector<const EnchantStat*>& enchantStats);

	/** C++ only: Ref<StatOwner> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	void endEffect(gameobjects::player::Player& player);

	items::ItemSlot getItemSlot() const { return itemSlot; }
};

} // namespace aion::gameserver::model::enchants
