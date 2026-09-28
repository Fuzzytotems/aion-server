#pragma once

#include <cstdint>

#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model {

/**
 * Objects with an expiration time (items, house objects, emotions, motions, titles, pets), checked by ExpireTimerTask.
 * <p>
 * C++: an interface held by `Ref<Expirable>` (fieldmap: ExpireTimerTask's map), so it declares the reference count operations
 * (docs/design/hub-headers.md §9.2); the first implementor of each hierarchy with a runtime base forwards them. Written with the S0b objects
 * group because Item (a hub) implements it.
 *
 * @author Mr. Poke
 */
class Expirable {
public:
	virtual int32_t getExpireTime() = 0;

	/** Java default method */
	virtual int32_t secondsUntilExpiration();

	/** Java default method */
	virtual bool isExpired();

	/** Java default method (does nothing) */
	virtual void onBeforeExpire(gameobjects::player::Player& player, int32_t remainingMinutes);

	virtual void onExpire(gameobjects::player::Player& player) = 0;

	/** Java default method (true) */
	virtual bool canExpireNow();

	/** C++ only: Ref<Expirable> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	virtual void release() const noexcept = 0;

	virtual ~Expirable() = default;

protected:
	Expirable() = default;
	Expirable(const Expirable&) = default;
	Expirable& operator=(const Expirable&) = default;
};

} // namespace aion::gameserver::model
