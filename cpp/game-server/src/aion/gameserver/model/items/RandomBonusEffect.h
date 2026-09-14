#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/templates/item/bonuses/fwd.h"

namespace aion::gameserver::model::items {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Item.bonusStatsEffect`, `IdianStone.rndBonusEffect`),
 * created with create(). StatOwner is held by Ref, so retain()/release() forward to RefCounted (§9.2). The constructor copies the modifiers of
 * the random bonus template (DataManager), so it stays `AION_UNPORTED` after the member initializers.
 *
 * @author xTz
 */
class RandomBonusEffect : public runtime::RefCounted, public stats::calc::StatOwner {
	AION_MAKE_REF_FRIEND
private:
	const int32_t statBonusId;
	runtime::ArrayList<const stats::calc::functions::StatFunction*> stats{AION_LOCK_CLASS(RandomBonusEffect::stats)};

protected:
	RandomBonusEffect(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId, int32_t statBonusId);
	~RandomBonusEffect() override;

public:
	/** Java: new RandomBonusEffect(type, statBonusSetId, statBonusId) */
	static runtime::Ref<RandomBonusEffect> create(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId, int32_t statBonusId);

	/** C++ only: Ref<StatOwner> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	int32_t getStatBonusId() const { return statBonusId; }

	void applyEffect(gameobjects::player::Player& player);

	void endEffect(gameobjects::player::Player& player);
};

} // namespace aion::gameserver::model::items
