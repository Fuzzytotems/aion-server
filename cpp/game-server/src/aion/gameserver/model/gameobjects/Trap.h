#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * <p>
 * C++ only: getMasterName() keeps Java's "" (the constructor sets an empty master name, which Npc's `Field<std::string>` cannot tell from null),
 * like Homing, Servant and SummonedHouseNpc (header request objects-1).
 *
 * @author ATracer
 */
class Trap : public SummonedObject {
	AION_MAKE_REF_FRIEND
protected:
	Trap(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, Creature& creator);

	void setupStatContainers() override;

public:
	int8_t getLevel() override;

	NpcObjectType getNpcObjectType() override;

	/** C++ only: Java's master name "" set by the constructor (see the class comment) */
	std::optional<std::string> getMasterName() override;

protected:
	~Trap() override;
};

} // namespace aion::gameserver::model::gameobjects
