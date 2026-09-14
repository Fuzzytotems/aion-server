#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5; the base of Kisk, written by the model-a lane because Kisk.h needs it). Java
 * `SummonedObject<T extends VisibleObject>` is one non-template class (§8.1): T is spelled VisibleObject, so getCreator() keeps Npc's
 * `Ptr<VisibleObject>` and subclasses cast. A visible object: `VisibleObject::create<SummonedObject>(...)` (§10.1). The creator may be null
 * (Kisk passes null). The Java constructor passes `DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId())` to Npc, so the base
 * initializer reaches `AION_UNPORTED` (a local helper in the .cpp).
 *
 * @author ATracer
 */
class SummonedObject : public Npc {
	AION_MAKE_REF_FRIEND
private:
	const int8_t level;
	const runtime::Ref<VisibleObject> creator;

protected:
	SummonedObject(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
		runtime::Ptr<VisibleObject> creator);
	~SummonedObject() override;

	void setupStatContainers() override;

public:
	int8_t getLevel() override;

	/** Java return type T (the creator, null if none) */
	runtime::Ptr<VisibleObject> getCreator() override;

	std::optional<std::string> getMasterName() override;

	int32_t getCreatorId() override;

	/** Java final */
	runtime::Ptr<Creature> getMaster() override final;

	CreatureType getType(Creature& creature) override;

	bool isEnemy(Creature& creature) override;

	/** C++: keeps Npc::isEnemyFrom(Creature&) visible next to the overrides (no hiding in Java) */
	using Npc::isEnemyFrom;

	bool isEnemyFrom(Npc& npc) override;

	bool isEnemyFrom(player::Player& player) override;

	Race getRace() override;

	bool isPvpTarget(Creature& creature) override;
};

} // namespace aion::gameserver::model::gameobjects
