#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * The damage an attacker (a creature or a team) dealt.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the element type of DamageList and TeamDamageList. K5 confined value class
 * (fieldmap). Java `DamageInfo<T extends AionObject>` is erased to one class (§8.1): the attacker is an AionObject (callers cast, e.g. to
 * Creature or Player). The attacker is a borrow valid within the task that built the list.
 */
class DamageInfo {
private:
	runtime::Ptr<model::gameobjects::AionObject> attacker;
	int32_t damage = 0;

public:
	explicit DamageInfo(model::gameobjects::AionObject& attacker);

	runtime::Ptr<model::gameobjects::AionObject> getAttacker() const { return attacker; }

	int32_t getDamage() const { return damage; }

	/** Java package-private */
	void addDamage(int32_t damage);
};

} // namespace aion::gameserver::controllers::attack
