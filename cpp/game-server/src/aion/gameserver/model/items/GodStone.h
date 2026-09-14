#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/items/ItemStone.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::items {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Item.godStone`), created with create(); retain()/release()
 * of the ItemStone interfaces forward to RefCounted (§9.2). The ItemStone constructor is unported (template check).
 *
 * @author ATracer
 */
class GodStone : public runtime::RefCounted, public ItemStone {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicLong cooldownExpireTimeMillis{AION_LOCK_CLASS(GodStone::cooldownExpireTimeMillis)};
	const templates::item::GodstoneInfo* godstoneInfo;
	runtime::Field<int32_t> activatedCount;

protected:
	GodStone(gameobjects::Item& parentItem, int32_t activatedCount, int32_t itemId, const templates::item::GodstoneInfo* godstoneInfo,
		PersistentState state);
	~GodStone() override;

public:
	/** Java: new GodStone(parentItem, activatedCount, itemId, godstoneInfo, state) */
	static runtime::Ref<GodStone> create(gameobjects::Item& parentItem, int32_t activatedCount, int32_t itemId,
		const templates::item::GodstoneInfo* godstoneInfo, PersistentState state);

	/** C++ only: Ref<StatOwner> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	const templates::item::GodstoneInfo* getGodstoneInfo() const { return godstoneInfo; }

	void increaseActivatedCount();

	int32_t getActivatedCount() const { return activatedCount.get(); }

	/** @return true if the godstone procs (the evaluation cooldown and the target's proc reduction apply) */
	bool tryActivate(bool isMainHandWeapon, gameobjects::Creature& target);
};

} // namespace aion::gameserver::model::items
