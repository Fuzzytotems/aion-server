#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/motion/fwd.h"

namespace aion::gameserver::model::gameobjects::player::motion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`PartSlot<MotionList>`, cycles review, set with setMotions), bound to
 * the player in the constructor. Both maps are created lazily (`Field<Ref<RcLinkedHashMap>>`); the getters return them as `Ptr`, null where
 * Java returns `Collections.emptyMap()`.
 *
 * @author MrPoke
 */
class MotionList : public runtime::OwnedPart {
private:
	runtime::OwnerRef<Player> owner;
	runtime::Field<runtime::Ref<runtime::RcLinkedHashMap<int32_t, runtime::Ref<Motion>>>> activeMotions{};
	runtime::Field<runtime::Ref<runtime::RcLinkedHashMap<int32_t, runtime::Ref<Motion>>>> motions{};

public:
	explicit MotionList(Player& owner);

	~MotionList() override;

	/** @return the active motions, null before the first one (Java: an empty map) */
	runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<Motion>>> getActiveMotions() const { return activeMotions.get(); }

	/** @return the motions, null before the first one (Java: an empty map) */
	runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<Motion>>> getMotions() const { return motions.get(); }

	void add(Motion& motion, bool persist);

	bool remove(int32_t motionId);

	void setActive(int32_t motionId, int32_t motionType);
};

} // namespace aion::gameserver::model::gameobjects::player::motion
