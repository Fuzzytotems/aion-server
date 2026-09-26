#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * Stores cooldowns by ID and removes them if they expired.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.craftCooldowns`/`houseObjectCooldowns`), created
 * with create(). Java `extends ConcurrentHashMap<Integer, Long>`: the C++ class derives the map shim like `runtime::Rc` does, and monitor() is
 * the shim's (as in Rc). get and put hide the shim functions of the same name (Java overrides them; no caller uses a ConcurrentHashMap
 * reference); Java returns null for a missing value: `std::optional<int64_t>`. `serialVersionUID` is dropped (no Java serialization).
 *
 * @author Neon
 */
class Cooldowns : public runtime::RefCounted, public runtime::ConcurrentHashMap<int32_t, int64_t> {
	AION_MAKE_REF_FRIEND
protected:
	Cooldowns();
	~Cooldowns() override;

public:
	/** Java: new Cooldowns() */
	static runtime::Ref<Cooldowns> create();

	runtime::Monitor& monitor() const noexcept { return runtime::ConcurrentHashMap<int32_t, int64_t>::monitor(); }

	/** @return the reuse time, null if there is none or it expired (an expired entry is removed) */
	std::optional<int64_t> get(int32_t cooldownId);

	/** @return the previous reuse time (null if none); a reuse time that already passed removes the entry instead */
	std::optional<int64_t> put(int32_t cooldownId, int64_t reuseTimeMillis);

	bool hasCooldown(int32_t cooldownId);

	int32_t remainingSeconds(int32_t cooldownId);
};

} // namespace aion::gameserver::model::gameobjects::player
