#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `PortalCooldownList.portalCooldowns`), created with
 * create().
 *
 * @author ViAl
 */
class PortalCooldown : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t worldId;
	const int64_t reuseTime;
	runtime::Field<int32_t> enterCount;

protected:
	PortalCooldown(int32_t worldId, int64_t reuseTime, int32_t enterCount);
	~PortalCooldown() override;

public:
	/** Java: new PortalCooldown(worldId, reuseTime, enterCount) */
	static runtime::Ref<PortalCooldown> create(int32_t worldId, int64_t reuseTime, int32_t enterCount);

	void increaseEnterCount();

	void decreaseEnterCount(int32_t count);

	int32_t getWorldId() const { return worldId; }

	int64_t getReuseTime() const { return reuseTime; }

	int32_t getEnterCount() const { return enterCount.get(); }
};

} // namespace aion::gameserver::model::gameobjects::player
