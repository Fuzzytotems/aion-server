#pragma once

#include <memory>

#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A group gate summoned by a creature (Java `SummonedObject<Creature>`, erased: hub-headers.md §8.1). A visible object:
 * `VisibleObject::create<GroupGate>(controller, spawnTemplate, creator)` (§10.1). The SummonedObject base initializer reads the NPC template
 * (DataManager, not ported yet), so the constructor reaches `AION_UNPORTED`.
 *
 * @author LokiReborn
 */
class GroupGate : public SummonedObject {
	AION_MAKE_REF_FRIEND
protected:
	GroupGate(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, Creature& creator);
	~GroupGate() override;

public:
	/** @return NpcObjectType.GROUPGATE */
	NpcObjectType getNpcObjectType() override;
};

} // namespace aion::gameserver::model::gameobjects
