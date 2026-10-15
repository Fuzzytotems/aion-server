#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * Java generic HouseObject<T>: erased to a non-template class, type variables spelled as their bounds (hub-headers.md §8.1).
 *
 * @author Rolandas
 */
class HouseObject : public VisibleObject, public Expirable, public Persistable {
private:
	runtime::Field<int32_t> expireEnd{};
	runtime::Field<float> x{};
	runtime::Field<float> y{};
	runtime::Field<float> z{};
	runtime::Field<int8_t> heading{};
	runtime::Field<int32_t> ownerUsedCount{0};
	runtime::Field<int32_t> visitorUsedCount{0};
	runtime::Field<std::optional<int32_t>> color{};
	runtime::Field<int32_t> colorExpireEnd{};
	const runtime::Ref<house::HouseRegistry> registry;
	runtime::Field<Persistable::PersistentState> persistentState{Persistable::PersistentState::NEW};

protected:
	HouseObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);

	HouseObject(CreateKey key, runtime::Ptr<house::HouseRegistry> registry, int32_t objId, int32_t templateId, bool autoReleaseObjectId);

public:
	/** C++ only: Expirable is held by Ref (ExpireTimerTask, hub-headers.md §9.2); retains the house object itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	/** C++ only: see retain() */
	void release() const noexcept override { runtime::RefCounted::release(); }

	Persistable::PersistentState getPersistentState() override { return this->persistentState.get(); }

	void setPersistentState(Persistable::PersistentState persistentState) override;

	int32_t getExpireTime() override { return this->expireEnd.get(); }

	void setExpireTime(int32_t time) { this->expireEnd.set(time); }

	void onExpire(player::Player& player) override;

protected:
	void despawnAndRemoveHouseObject(player::Player& player, bool isExpired);

public:
	const templates::housing::PlaceableHouseObject* getObjectTemplate() const; // narrows VisibleObject::getObjectTemplate (Java cast-only override)

	float getX() override { return this->x.get(); }

	void setX(float x);

	float getY() override { return this->y.get(); }

	void setY(float y);

	float getZ() override { return this->z.get(); }

	void setZ(float z);

	int8_t getHeading() override { return this->heading.get(); }

	void setHeading(int8_t heading);

	int32_t getRotation();

	void setRotation(int32_t rotation);

	templates::housing::PlaceLocation getPlaceLocation();

	templates::housing::PlaceArea getPlaceArea();

	int32_t getPlacementLimit(bool trial);

	templates::item::ItemQuality getQuality();

	float getTalkingDistance();

	templates::housing::HousingCategory getCategory();

	runtime::Ptr<house::HouseRegistry> getRegistry() const { return this->registry; }

	runtime::Ptr<house::House> getOwnerHouse();

	int32_t getPlayerId();

	int32_t getOwnerUsedCount() const { return this->ownerUsedCount.get(); }

	void incrementOwnerUsedCount();

	void incrementVisitorUsedCount();

	void setOwnerUsedCount(int32_t ownerUsedCount);

	int32_t getVisitorUsedCount() const { return this->visitorUsedCount.get(); }

	void setVisitorUsedCount(int32_t visitorUsedCount);

	/** Means the player has it spawned, not the game server */
	bool isSpawnedByPlayer();

	controllers::PlaceableObjectController& getController() const; // narrows VisibleObject::getController (Java cast-only override)

	virtual void spawn();

	/** Removes house from spawn but it remains in registry */
	void removeFromHouse();

	virtual void onUse(player::Player& player);

	void onDialogRequest(player::Player& player);

	virtual void onDespawn();

	std::optional<int32_t> getColor() const { return this->color.get(); }

	void setColor(std::optional<int32_t> color);

	int32_t getColorExpireEnd() const { return this->colorExpireEnd.get(); }

	void setColorExpireEnd(int32_t value) { this->colorExpireEnd.set(value); }
};

} // namespace aion::gameserver::model::gameobjects
