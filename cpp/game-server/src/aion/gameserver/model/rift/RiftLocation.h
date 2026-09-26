#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/rift/fwd.h"
#include "aion/gameserver/model/templates/rift/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::rift {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Source
 */
class RiftLocation : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<bool> opened{};
	const templates::rift::RiftTemplate* template_;
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<templates::spawns::SpawnTemplate>> spawned{AION_LOCK_CLASS(RiftLocation::spawned#stripe)};

protected:
	explicit RiftLocation(const templates::rift::RiftTemplate* template_);

public:
	static runtime::Ref<RiftLocation> create(const templates::rift::RiftTemplate* value);

	int32_t getId();

	int32_t getWorldId();

	bool hasSpawns();

	bool isAutoCloseable();

	bool isOpened() const { return this->opened.get(); }

	void setOpened(bool state) { this->opened.set(state); }

	runtime::ConcurrentHashMap<int32_t, runtime::Ref<templates::spawns::SpawnTemplate>>& getSpawned() { return this->spawned; }

	void addSpawned(gameobjects::VisibleObject& object);

	bool replaceSpawned(int32_t oldObjectId, gameobjects::VisibleObject& newObject);

protected:
	~RiftLocation() override;
};

} // namespace aion::gameserver::model::rift
