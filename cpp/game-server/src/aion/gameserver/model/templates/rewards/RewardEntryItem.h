#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::model::templates::rewards {

/**
 * Java com.aionemu.gameserver.model.templates.rewards.RewardEntryItem: a reward item of the web reward table.
 * <p>
 * C++: RefCounted like RewardItem (fieldmap K3 in its class tree), created with create().
 *
 * @author KID, Neon
 */
class RewardEntryItem : public RewardItem {
	AION_MAKE_REF_FRIEND
private:
	const int32_t entryId;

protected:
	RewardEntryItem(int32_t entryId, int32_t itemId, int64_t count);
	~RewardEntryItem() override;

public:
	/** Java: new RewardEntryItem(entryId, itemId, count) */
	static runtime::Ref<RewardEntryItem> create(int32_t entryId, int32_t itemId, int64_t count);

	int32_t getEntryId() const { return entryId; }
};

} // namespace aion::gameserver::model::templates::rewards
