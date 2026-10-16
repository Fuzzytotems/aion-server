#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode_DespawnableType.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine::scene {

/**
 * A scene node that is active only in some instances or states (doors, placeables, town objects, siege shields, event objects).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): an element type of GeoMap. RefCounted (fieldmap K4), created with create().
 * `instances` is Java's BitSet (no runtime shim, fieldmap flag noShim): the set of the indexes whose bit is set, guarded by its own Monitor
 * (Java `synchronized (instances)`).
 */
class DespawnableNode : public Node {
	AION_MAKE_REF_FRIEND
public:
	using DespawnableType = DespawnableNode_DespawnableType;

	runtime::Field<DespawnableType> type{DespawnableType::NONE};
	runtime::Field<int32_t> id{0};
	runtime::Field<int8_t> levelBitMask{0};

private:
	// fieldmap.toml: java.util.BitSet (no shim) as the set bit indexes
	runtime::HashSet<int32_t> instances{AION_LOCK_CLASS(DespawnableNode::instances)};

protected:
	DespawnableNode();
	~DespawnableNode() override;

public:
	/** Java: new DespawnableNode() */
	static runtime::Ref<DespawnableNode> create();

	void setActive(int32_t instanceId, bool active);

	bool isActive(int32_t instanceId);

	/** @throws CloneNotSupportedException (Java) for children that are neither Geometry nor Node */
	void copyFrom(Node& node);

	int32_t collideWith(math::Ray& other, collision::CollisionResults& results) override;

	/** Java: Node clone() (covariant return) */
	runtime::Ref<Spatial> clone() override;

	void setType(DespawnableType value) { type.set(value); }

	void setId(int32_t value) { id.set(value); }
};

} // namespace aion::gameserver::geoEngine::scene
