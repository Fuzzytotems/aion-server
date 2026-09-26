#pragma once

#include <cstdint>
#include <memory>
#include <set>

#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/staticdoor/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A door of a map or instance. A visible object: `VisibleObject::create<StaticDoor>(controller, spawnTemplate, objectTemplate, instanceId)`
 * (§10.1).
 * <p>
 * Java's `EnumSet<StaticDoorState> states` (fieldmap: no shim for EnumSet) is a TreeSet shim, which iterates in ordinal order like an EnumSet and
 * guards each operation with its Monitor; getStates() returns a snapshot (hub-headers.md §6: EnumSet in signatures is std::set).
 *
 * @author MrPoke, Rolandas
 */
class StaticDoor : public StaticObject {
	AION_MAKE_REF_FRIEND
private:
	// fieldmap: EnumSet has no runtime shim; a TreeSet iterates in ordinal order like EnumSet (a fieldmap.toml decision is requested)
	runtime::TreeSet<templates::staticdoor::StaticDoorState> states{AION_LOCK_CLASS(StaticDoor::states)}; // Java: EnumSet.noneOf (constructor)
	runtime::Field<bool> isLocked_{true};

protected:
	StaticDoor(CreateKey key, std::unique_ptr<controllers::StaticObjectController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::staticdoor::StaticDoorTemplate* objectTemplate, int32_t instanceId);
	~StaticDoor() override;

public:
	bool isLocked() const { return isLocked_.get(); }

	void setLocked(bool value) { isLocked_.set(value); }

	/** @return the open state from states set */
	bool isOpen();

	/** @return a snapshot of the states (Java returns the live EnumSet; its only caller formats it) */
	std::set<templates::staticdoor::StaticDoorState> getStates();

	/** @param open the open state to set */
	void setOpen(bool open);

	void changeState(bool open, int32_t state);

	/** Narrows VisibleObject::getObjectTemplate (Java cast-only override) */
	const templates::staticdoor::StaticDoorTemplate* getObjectTemplate() const;
};

} // namespace aion::gameserver::model::gameobjects
