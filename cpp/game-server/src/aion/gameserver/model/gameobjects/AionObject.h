#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * This is the base class for all "in-game" objects, that player can interact with, such as: npcs, monsters, players, items.<br>
 * <br>
 * Each AionObject is uniquely identified by objectId.
 * <p>
 * Hub header (docs/design/hub-headers.md, reference implementation). RefCounted (runtime-architecture.md §2.2): created only through
 * `X::create(...)` or, for visible objects, `VisibleObject::create<T>(...)`; the destructor is protected and release-only.
 * Java's `Cleaner` registration for auto-release ids becomes CleanerQueue::push in the destructor (runtime-architecture.md §6 group (a)).
 *
 * @author -Nemesiss-, SoulKeeper, Neon
 */
class AionObject : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	// Java: private static final Cleaner CLEANER - replaced by runtime::CleanerQueue (no member)
	/** Unique id, for all game objects such as: items, players, monsters. */
	const int32_t objectId;
	/** C++ only: Java registers objId with CLEANER when `objectId != 0 && autoReleaseObjectId` (AionObject.java:31-34) */
	const bool autoReleaseObjectId; // fieldmap: C++-only replacement of Cleaner.register (runtime-architecture.md §6)

protected:
	explicit AionObject(int32_t objId);
	AionObject(int32_t objId, bool autoReleaseObjectId);
	/** Release-only: pushes an auto-release objectId to runtime::CleanerQueue (RespawnService.setAutoReleaseId / IDFactory.releaseId). */
	~AionObject() override;

public:
	/** Returns unique ObjectId of AionObject */
	int32_t getObjectId() const { return objectId; }

	/** Java final: the objectId (the collection shims hash through it, runtime/collections/JavaEquals.h) */
	int32_t hashCode() const { return objectId; }

	/** Java final: equal objectIds; a dummy object (objectId 0) equals only itself */
	bool equals(const AionObject& obj) const {
		if (this == &obj)
			return true;
		if (objectId == 0) // object is a dummy (no unique ID from IDFactory)
			return false;
		return hashCode() == obj.hashCode(); // cheap direct objectId comparison (see above)
	}

	/**
	 * Returns name of the object.<br>
	 * Unique for players, common for NPCs, items, etc
	 */
	virtual std::string getName() = 0;

	virtual std::string toString();
};

} // namespace aion::gameserver::model::gameobjects
