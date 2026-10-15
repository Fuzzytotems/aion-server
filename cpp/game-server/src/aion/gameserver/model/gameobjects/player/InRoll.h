#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.inRoll`), created with create().
 *
 * @author xTz
 */
class InRoll : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> npcId;
	runtime::Field<int32_t> itemId;
	runtime::Field<int32_t> rollType;
	runtime::Field<int32_t> index;

protected:
	InRoll(int32_t npcId, int32_t itemId, int32_t index, int32_t rollType);
	~InRoll() override;

public:
	/** Java: new InRoll(npcId, itemId, index, rollType) */
	static runtime::Ref<InRoll> create(int32_t npcId, int32_t itemId, int32_t index, int32_t rollType);

	int32_t getNpcId() const { return npcId.get(); }

	int32_t getItemId() const { return itemId.get(); }

	int32_t getIndex() const { return index.get(); }

	int32_t getRollType() const { return rollType.get(); }

	void setNpcId(int32_t value) { npcId.set(value); }

	void setItemId(int32_t value) { itemId.set(value); }

	/** Java body: `this.index = itemId` (stores the item id, not the argument) */
	void setIndexd(int32_t value);

	void setRollType(int32_t value) { rollType.set(value); }
};

} // namespace aion::gameserver::model::gameobjects::player
