#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A house object of the HousingEmblem template (Java `HouseObject<HousingEmblem>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<EmblemObject>(registry, objId, templateId)` (§10.1); the HouseObject constructor binds the controller and the known list.
 *
 * @author Rolandas
 */
class EmblemObject : public HouseObject {
	AION_MAKE_REF_FRIEND
protected:
	EmblemObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);
	~EmblemObject() override;

public:
	/** Narrows HouseObject::getObjectTemplate (Java type variable T bound to HousingEmblem, §8.2) */
	const templates::housing::HousingEmblem* getObjectTemplate() const;
	bool canExpireNow() override;
};

} // namespace aion::gameserver::model::gameobjects
