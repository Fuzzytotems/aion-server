#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/UseableHouseObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A house storage (cabinet) (Java `UseableHouseObject<HousingStorage>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<StorageObject>(registry, objId, templateId)` (§10.1).
 *
 * @author Rolandas, Neon
 */
class StorageObject : public UseableHouseObject {
	AION_MAKE_REF_FRIEND
protected:
	StorageObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);
	~StorageObject() override;

public:
	/** Narrows HouseObject::getObjectTemplate (Java type variable T bound to HousingStorage, §8.2) */
	const templates::housing::HousingStorage* getObjectTemplate() const;

	void onUse(player::Player& player) override;
};

} // namespace aion::gameserver::model::gameobjects
