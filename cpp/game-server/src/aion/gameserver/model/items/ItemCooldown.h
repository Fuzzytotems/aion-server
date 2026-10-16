#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/items/fwd.h"

namespace aion::gameserver::model::items {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K3, `Player.itemCoolDowns`), created with create().
 *
 * @author ATracer
 */
class ItemCooldown : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int64_t time;
	const int32_t useDelay;

protected:
	ItemCooldown(int64_t time, int32_t useDelay);
	~ItemCooldown() override;

public:
	/** Java: new ItemCooldown(time, useDelay) */
	static runtime::Ref<ItemCooldown> create(int64_t time, int32_t useDelay);

	int64_t getReuseTime() const { return time; }

	int32_t getUseDelay() const { return useDelay; }
};

} // namespace aion::gameserver::model::items
