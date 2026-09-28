#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"

namespace aion::gameserver::model::templates::rewards {

/**
 * Java record com.aionemu.gameserver.model.templates.rewards.ArenaRewardItem(int itemId, int baseCount, int rankingCount, int scoreCount).
 * <p>
 * C++: RefCounted (fieldmap K3, member of PvPArenaPlayerReward), created with create(). The record components are const members with a trailing
 * underscore, since the accessors keep the component names; equals, hashCode and toString follow the record contract.
 *
 * @author Estrayl
 */
class ArenaRewardItem : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t itemId_;
	const int32_t baseCount_;
	const int32_t rankingCount_;
	const int32_t scoreCount_;

protected:
	ArenaRewardItem(int32_t itemId, int32_t baseCount, int32_t rankingCount, int32_t scoreCount);
	~ArenaRewardItem() override;

public:
	/** Java: new ArenaRewardItem(itemId, baseCount, rankingCount, scoreCount) */
	static runtime::Ref<ArenaRewardItem> create(int32_t itemId, int32_t baseCount, int32_t rankingCount, int32_t scoreCount);

	int32_t itemId() const { return itemId_; }

	int32_t baseCount() const { return baseCount_; }

	int32_t rankingCount() const { return rankingCount_; }

	int32_t scoreCount() const { return scoreCount_; }

	int32_t getTotalCount() const;

	bool equals(const ArenaRewardItem& obj) const;

	int32_t hashCode() const;

	std::string toString();
};

} // namespace aion::gameserver::model::templates::rewards
