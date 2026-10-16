#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/UseableHouseObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * The house postbox (Java `UseableHouseObject<HousingPostbox>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<PostboxObject>(registry, objId, templateId)` (§10.1).
 *
 * @author Rolandas, Neon
 */
class PostboxObject : public UseableHouseObject {
	AION_MAKE_REF_FRIEND
protected:
	PostboxObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);
	~PostboxObject() override;

public:
	/** Narrows HouseObject::getObjectTemplate (Java type variable T bound to HousingPostbox, §8.2) */
	const templates::housing::HousingPostbox* getObjectTemplate() const;

	void onUse(player::Player& player) override;
};

} // namespace aion::gameserver::model::gameobjects
