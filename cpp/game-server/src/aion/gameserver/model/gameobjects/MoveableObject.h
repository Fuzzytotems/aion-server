#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A house object of the HousingMoveableItem template (Java `HouseObject<HousingMoveableItem>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<MoveableObject>(registry, objId, templateId)` (§10.1); the HouseObject constructor binds the controller and the known list.
 *
 * @author Rolandas
 */
class MoveableObject : public HouseObject {
	AION_MAKE_REF_FRIEND
protected:
	MoveableObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);
	~MoveableObject() override;

public:
	/** Narrows HouseObject::getObjectTemplate (Java type variable T bound to HousingMoveableItem, §8.2) */
	const templates::housing::HousingMoveableItem* getObjectTemplate() const;
	void onUse(player::Player& player) override;
};

} // namespace aion::gameserver::model::gameobjects
