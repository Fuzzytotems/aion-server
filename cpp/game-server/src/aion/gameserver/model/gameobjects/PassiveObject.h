#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A house object of the HousingPassiveItem template (Java `HouseObject<HousingPassiveItem>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<PassiveObject>(registry, objId, templateId)` (§10.1); the HouseObject constructor binds the controller and the known list.
 *
 * @author Rolandas
 */
class PassiveObject : public HouseObject {
	AION_MAKE_REF_FRIEND
protected:
	PassiveObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);
	~PassiveObject() override;

public:
	/** Narrows HouseObject::getObjectTemplate (Java type variable T bound to HousingPassiveItem, §8.2) */
	const templates::housing::HousingPassiveItem* getObjectTemplate() const;
};

} // namespace aion::gameserver::model::gameobjects
