#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A house object a single player uses at a time (postbox, storage, useable items).
 * <p>
 * Java generic `UseableHouseObject<T extends PlaceableHouseObject>`: one non-template class (hub-headers.md §8.1), the template stays
 * HouseObject::getObjectTemplate()'s PlaceableHouseObject. Java abstract: the constructor is protected (§9.1).
 *
 * @author Neon
 */
class UseableHouseObject : public HouseObject {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicInteger usingPlayer{AION_LOCK_CLASS(UseableHouseObject::usingPlayer)}; // Java: = new AtomicInteger()

protected:
	UseableHouseObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);
	~UseableHouseObject() override;

public:
	bool canExpireNow() override;

	/** @return True if a player currently uses this object and no one else should access it. */
	bool isOccupied();

	/** Java final. @return True if the player could successful occupy the object or is already occupying it */
	bool setOccupant(player::Player& player);

	/** Java final. @return True if the using player was released and the object is not occupied anymore. */
	bool releaseOccupant(player::Player& player);

protected:
	/** Java final: unsets the using player without any checks. For internal use only. */
	void releaseOccupant();

public:
	virtual bool hasUseCooldown();
};

} // namespace aion::gameserver::model::gameobjects
