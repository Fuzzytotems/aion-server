#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::house {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `House.houseRegistry`, `HouseObject.registry`), created with
 * create(). The owner is retained (fieldmap `const Ref<House>`; the House <-> registry edge is resolved in cycles.toml, S0B-098). Java
 * `HouseObject<?>` is the erased HouseObject (§8.1). getUsedDecorId returns Java's Integer (null if the building has no default part):
 * `std::optional<int32_t>`. The generic helper discard is a member template with an inline stub (§8.3).
 *
 * @author Rolandas
 */
class HouseRegistry : public runtime::RefCounted, public gameobjects::Persistable {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<House> owner;
	runtime::LinkedHashMap<int32_t, runtime::Ref<gameobjects::HouseObject>> objects{AION_LOCK_CLASS(HouseRegistry::objects)};
	runtime::LinkedHashMap<int32_t, runtime::Ref<gameobjects::HouseDecoration>> decors{AION_LOCK_CLASS(HouseRegistry::decors)};
	runtime::Field<PersistentState> persistentState{PersistentState::UPDATED};

protected:
	explicit HouseRegistry(House& owner);
	~HouseRegistry() override;

public:
	/** Java: new HouseRegistry(owner) */
	static runtime::Ref<HouseRegistry> create(House& owner);

	runtime::Ptr<House> getOwner() const { return owner; }

	std::vector<runtime::Ptr<gameobjects::HouseObject>> getObjects();

	std::vector<runtime::Ptr<gameobjects::HouseObject>> getSpawnedObjects();

	std::vector<runtime::Ptr<gameobjects::HouseObject>> getNotSpawnedObjects();

	/** @return the object, null if the registry has none with that id */
	runtime::Ptr<gameobjects::HouseObject> getObjectByObjId(int32_t itemObjId);

	bool putObject(gameobjects::HouseObject& houseObject, bool saveRegistry);

	void discardObject(gameobjects::HouseObject& object, bool direct);

	std::vector<runtime::Ptr<gameobjects::HouseDecoration>> getDecors();

	std::vector<runtime::Ptr<gameobjects::HouseDecoration>> getUnusedDecors();

	/** @return the decoration, null if the registry has none with that id */
	runtime::Ptr<gameobjects::HouseDecoration> getDecorByObjId(int32_t itemObjId);

	bool putDecor(gameobjects::HouseDecoration& decor, bool saveRegistry);

	std::optional<int32_t> getUsedDecorId(templates::housing::PartType partType, int32_t room);

	void setUsed(gameobjects::HouseDecoration& decor, int32_t room);

	void discardDecor(templates::housing::PartType partType, int32_t roomNo);

	void discardDecor(gameobjects::HouseDecoration& decor, bool direct);

private:
	/** Java: private <T extends AionObject & Persistable> void discard(Map<Integer, T> map, T obj, boolean direct) */
	template <class T>
	void discard(runtime::LinkedHashMap<int32_t, runtime::Ref<T>>& map, T& obj, bool direct) {
		AION_UNPORTED();
	}

public:
	void reset();

	void save();

	/** Java final */
	PersistentState getPersistentState() override final { return persistentState.get(); }

	/** Java final */
	void setPersistentState(PersistentState value) override final { persistentState.set(value); }

	int32_t size();
};

} // namespace aion::gameserver::model::house
