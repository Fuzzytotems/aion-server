#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/knownlist/fwd.h"

namespace aion::gameserver::world::knownlist {

/**
 * An object known by a KnownList, with its cached visibility state.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `KnownList::knownObjects`). equals/hashCode compare the
 * known object (Java `Objects.equals(object, that.object)`, AionObject equality by objectId) and are ported for the collection shims.
 */
class KnownObject : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::VisibleObject> object;
	runtime::Field<bool> visible{};

protected:
	explicit KnownObject(model::gameobjects::VisibleObject& object);
	~KnownObject() override;

public:
	/** Java: new KnownObject(object) */
	static runtime::Ref<KnownObject> create(model::gameobjects::VisibleObject& object);

	runtime::Ptr<model::gameobjects::VisibleObject> get() const { return object; }

	bool isVisible() const { return visible.get(); }

	/** Java package-private; synchronized (this) */
	bool updateVisible(bool visible);

	bool equals(const KnownObject& o) const;

	int32_t hashCode() const;

	std::string toString();
};

} // namespace aion::gameserver::world::knownlist
