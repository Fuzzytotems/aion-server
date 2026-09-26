#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/model/templates/item/enums/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * Java com.aionemu.gameserver.skillengine.model.WeaponTypeWrapper: the (main hand, off hand) weapon group pair that keys MotionTime's
 * animation times; the constructor folds a dual-wield pair to the one group that has its own times.
 * <p>
 * C++: declarations of m5b2-plan.md S-07, the bodies are the cast lane's. RefCounted immutable value (fieldmap K3), created with
 * `WeaponTypeWrapper::create(...)`. MotionTime is Java's only user and does not use this class in C++: it keys its maps with the pair
 * `MotionTime::WeaponKey` that `MotionTime::weaponTypeWrapper` normalizes exactly like this constructor (MotionTime.h, P4-08). Nothing on the
 * weapon-condition path uses it (m5b2-plan.md S-07 inferred that; `grep WeaponTypeWrapper` over the Java tree finds MotionTime only).
 *
 * @author kecimis
 */
class WeaponTypeWrapper : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	// Java initializes both to null, the constructor keeps a null argument and compareTo/hashCode test for null, so both are optionals like
	// IgnoreProperties.race (header request geo-2); fieldmap prints `const ItemGroup`, a decision is requested (header request m5b2-fm-2).
	// m5b2-fm-2 (fieldmap.toml): a nullable enum, std::optional (hub-headers.md §6)
	const std::optional<gameserver::model::templates::item::enums::ItemGroup> mainHand{}; // Java: = null
	// m5b2-fm-2 (fieldmap.toml): a nullable enum, std::optional (hub-headers.md §6)
	const std::optional<gameserver::model::templates::item::enums::ItemGroup> offHand{}; // Java: = null

protected:
	/** `mainHand` and `offHand` are nullable (MotionTime passes null, e.g. `new WeaponTypeWrapper(ItemGroup.SWORD, null)`) */
	WeaponTypeWrapper(std::optional<gameserver::model::templates::item::enums::ItemGroup> mainHand,
		std::optional<gameserver::model::templates::item::enums::ItemGroup> offHand);

	~WeaponTypeWrapper() override;

public:
	/** Java `new WeaponTypeWrapper(mainHand, offHand)` */
	static runtime::Ref<WeaponTypeWrapper> create(std::optional<gameserver::model::templates::item::enums::ItemGroup> mainHand,
		std::optional<gameserver::model::templates::item::enums::ItemGroup> offHand);

	bool equals(const WeaponTypeWrapper& obj) const;

	std::string toString();

	int32_t hashCode() const;

	int32_t compareTo(const WeaponTypeWrapper& o) const;

	std::optional<gameserver::model::templates::item::enums::ItemGroup> getMainHand() const { return mainHand; }

	std::optional<gameserver::model::templates::item::enums::ItemGroup> getOffHand() const { return offHand; }
};

} // namespace aion::gameserver::skillengine::model
