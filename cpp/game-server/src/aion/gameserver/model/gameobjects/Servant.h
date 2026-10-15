#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A servant (totem, energy, ...) summoned by a creature (Java `SummonedObject<Creature>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<Servant>(controller, spawnTemplate, level, creator)` (§10.1). The SummonedObject base initializer reads the NPC template
 * (DataManager, not ported yet), so the constructor reaches `AION_UNPORTED`.
 * The object type is null in Java until setNpcObjectType (VisibleObjectSpawner sets it right after creation); the member keeps the fieldmap spelling
 * `Field<NpcObjectType>`, so it reads NORMAL before that.
 * C++ only: getMasterName() keeps Java's "" (the constructor sets an empty master name, which Npc's `Field<std::string>` cannot tell from null).
 *
 * @author ATracer
 */
class Servant : public SummonedObject {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<NpcObjectType> objectType{};

protected:
	Servant(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
		Creature& creator);
	~Servant() override;

	void setupStatContainers() override;

public:
	NpcObjectType getNpcObjectType() override;

	void setNpcObjectType(NpcObjectType value) { objectType.set(value); }

	void setUpStats();

	/** C++ only: Java's master name "" set by the constructor (see the class comment) */
	std::optional<std::string> getMasterName() override;
};

} // namespace aion::gameserver::model::gameobjects
