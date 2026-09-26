#pragma once

#include <cstdint>
#include <map>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/motion/fwd.h"

namespace aion::gameserver::model::gameobjects::player::motion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `MotionList.motions`), created with create(). Expirable is
 * held by Ref, so retain()/release() forward to RefCounted (§9.2). The motion type table is filled by Java's static initializer and never
 * changed afterwards: a constant `std::map` (its key order equals Java's insertion order), so no Monitor runs during static initialization.
 *
 * @author MrPoke
 */
class Motion : public runtime::RefCounted, public Expirable {
	AION_MAKE_REF_FRIEND
public:
	// fieldmap.toml: Java static final LinkedHashMap written only by the static initializer: a constant table (no shim Monitor at static init)
	static inline const std::map<int32_t, int32_t> motionType{{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 1}, {6, 2}, {7, 3}, {8, 4}, {9, 5}, {10, 5},
		{11, 1}, {12, 2}, {13, 3}, {14, 4}, {15, 1}, {16, 2}, {17, 3}, {18, 4}, {19, 5}, {20, 1}, {21, 1}, {22, 1}, {23, 1}, {24, 2}, {25, 4},
		{26, 3}};

private:
	const int32_t id;
	const int32_t deletionTime;
	runtime::Field<bool> active;

protected:
	Motion(int32_t id, int32_t deletionTime, bool isActive);
	~Motion() override;

public:
	/** Java: new Motion(id, deletionTime, isActive) */
	static runtime::Ref<Motion> create(int32_t id, int32_t deletionTime, bool isActive);

	/** C++ only: Ref<Expirable> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	/** @return the id */
	int32_t getId() const { return id; }

	/** @return the active */
	bool isActive() const { return active.get(); }

	/** @param value the active to set */
	void setActive(bool value) { active.set(value); }

	int32_t getExpireTime() override { return deletionTime; }

	void onExpire(Player& player) override;
};

} // namespace aion::gameserver::model::gameobjects::player::motion
