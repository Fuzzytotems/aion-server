#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A house object that spawns an npc while it is spawned (Java `HouseObject<HousingNpc>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<NpcObject>(registry, objId, templateId)` (§10.1). The npc field is cut by onDespawn (cycles.toml java-hook
 * `NpcObject.npc`).
 *
 * @author Rolandas
 */
class NpcObject : public HouseObject {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<Npc>> npc{}; // Java: = null

protected:
	NpcObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);
	~NpcObject() override;

public:
	/** Narrows HouseObject::getObjectTemplate (Java type variable T bound to HousingNpc, §8.2) */
	const templates::housing::HousingNpc* getObjectTemplate() const;

	void onUse(player::Player& player) override;

	void spawn() override; // synchronized

	void onDespawn() override; // synchronized

	bool canExpireNow() override; // synchronized

	int32_t getNpcObjectId();
};

} // namespace aion::gameserver::model::gameobjects
