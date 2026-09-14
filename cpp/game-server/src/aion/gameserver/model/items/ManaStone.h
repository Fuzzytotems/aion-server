#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/items/ItemStone.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"

namespace aion::gameserver::model::items {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Item.manaStones`/`fusionStones`), created with create();
 * retain()/release() of the ItemStone interfaces forward to RefCounted (§9.2). The constructor copies the modifiers of the stone's item
 * template (DataManager), so it stays `AION_UNPORTED` (the ItemStone constructor already is).
 *
 * @author ATracer
 */
class ManaStone : public runtime::RefCounted, public ItemStone {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<const stats::calc::functions::StatFunction*> modifiers{AION_LOCK_CLASS(ManaStone::modifiers)};

protected:
	ManaStone(int32_t itemObjId, int32_t itemId, int32_t slot, PersistentState persistentState);
	~ManaStone() override;

public:
	/** Java: new ManaStone(itemObjId, itemId, slot, persistentState) */
	static runtime::Ref<ManaStone> create(int32_t itemObjId, int32_t itemId, int32_t slot, PersistentState persistentState);

	/** C++ only: Ref<StatOwner> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	/** @return modifiers */
	runtime::ArrayList<const stats::calc::functions::StatFunction*>& getModifiers() { return modifiers; }

	/** @return the first modifier, null if the stone has none */
	const stats::calc::functions::StatFunction* getFirstModifier();
};

} // namespace aion::gameserver::model::items
