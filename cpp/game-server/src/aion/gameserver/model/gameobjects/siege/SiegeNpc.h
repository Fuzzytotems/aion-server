#pragma once

#include <cstdint>
#include <memory>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/siege/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/npc/fwd.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/fwd.h"

namespace aion::gameserver::model::gameobjects::siege {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A visible object: `VisibleObject::create<SiegeNpc>(controller, spawnTemplate,
 * objectTemplate)` (§10.1). getSpawn() is Java's cast-only override: a non-virtual narrowing redeclaration (§8.2).
 *
 * @author ViAl
 */
class SiegeNpc : public Npc {
	AION_MAKE_REF_FRIEND
protected:
	SiegeNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::siegespawns::SiegeSpawnTemplate& spawnTemplate,
		const templates::npc::NpcTemplate* objectTemplate);
	~SiegeNpc() override;

public:
	model::siege::SiegeRace getSiegeRace();

	int32_t getSiegeId();

	/** Narrows VisibleObject::getSpawn (Java cast-only override) */
	runtime::Ptr<templates::spawns::siegespawns::SiegeSpawnTemplate> getSpawn() const;

	/** C++: keeps Npc::isEnemyFrom(Npc&) and isEnemyFrom(Player&) visible next to the override (no hiding in Java) */
	using Npc::isEnemyFrom;

	bool isEnemyFrom(Creature& creature) override;
};

} // namespace aion::gameserver::model::gameobjects::siege
