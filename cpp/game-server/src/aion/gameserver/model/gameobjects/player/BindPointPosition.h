#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.bindPoint`), created with create().
 *
 * @author evilset
 */
class BindPointPosition : public runtime::RefCounted, public Persistable {
	AION_MAKE_REF_FRIEND
private:
	const int32_t mapId;
	const float x;
	const float y;
	const float z;
	const int8_t heading;
	runtime::Field<PersistentState> persistentState; // Java: = PersistentState.NEW (constructor)

protected:
	BindPointPosition(int32_t mapId, float x, float y, float z, int8_t heading);
	~BindPointPosition() override;

public:
	/** Java: new BindPointPosition(mapId, x, y, z, heading) */
	static runtime::Ref<BindPointPosition> create(int32_t mapId, float x, float y, float z, int8_t heading);

	int32_t getMapId() const { return mapId; }

	float getX() const { return x; }

	float getY() const { return y; }

	float getZ() const { return z; }

	int8_t getHeading() const { return heading; }

	PersistentState getPersistentState() override { return persistentState.get(); }

	void setPersistentState(PersistentState persistentState) override;
};

} // namespace aion::gameserver::model::gameobjects::player
