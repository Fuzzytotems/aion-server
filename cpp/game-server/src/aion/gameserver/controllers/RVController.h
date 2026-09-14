#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/services/rift/fwd.h"

namespace aion::gameserver::controllers {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer, Source, Sykra
 */
class RVController : public NpcController {
private:
	const bool isMaster_; // Java: = false
	const bool isVortex_; // Java: = false
	const bool isVolatile_; // Java: = false
	const bool isInvasion_; // Java: = false
	// Java: = new HashMap<>()
	runtime::HashMap<int32_t, runtime::Ref<model::gameobjects::player::Player>> passedPlayers{AION_LOCK_CLASS(RVController::passedPlayers)};
	const runtime::Ref<model::templates::spawns::SpawnTemplate> slaveSpawnTemplate;
	const runtime::Ref<model::gameobjects::Npc> slave;
	const std::optional<int32_t> maxEntries;
	const std::optional<int32_t> minLevel;
	const std::optional<int32_t> maxLevel;
	runtime::Field<int32_t> usedEntries{0};
	const bool isAccepting;
	const services::rift::RiftEnum riftTemplate;
	const int32_t deSpawnedTime;

public:
	/** Used to create master rifts or slave rifts (slave == null) */
	RVController(runtime::Ptr<model::gameobjects::Npc> slave, services::rift::RiftEnum riftTemplate);

	RVController(runtime::Ptr<model::gameobjects::Npc> slave, services::rift::RiftEnum riftTemplate, bool isWithGuards);

	void onDialogRequest(model::gameobjects::player::Player& player) override;

private:
	void onRequest(model::gameobjects::player::Player& player);

	bool onAccept(model::gameobjects::player::Player& player);

public:
	void onDespawn() override;

	bool isMaster() const { return this->isMaster_; }

	bool isVortex() const { return this->isVortex_; }

	std::optional<int32_t> getMaxEntries() const { return this->maxEntries; }

	std::optional<int32_t> getMinLevel() const { return this->minLevel; }

	std::optional<int32_t> getMaxLevel() const { return this->maxLevel; }

	services::rift::RiftEnum getRiftTemplate() const { return this->riftTemplate; }

	runtime::Ptr<model::gameobjects::Npc> getSlave() const { return this->slave; }

	int32_t getUsedEntries() const { return this->usedEntries.get(); }

	int32_t getRemainTime();

	bool isVolatile() const { return this->isVolatile_; }

	bool isInvasion() const { return this->isInvasion_; }

	runtime::HashMap<int32_t, runtime::Ref<model::gameobjects::player::Player>>& getPassedPlayers() { return this->passedPlayers; }

	void syncPassed(bool invasion);

private:
	std::vector<int32_t> getWorldsList(RVController& controller);
};

} // namespace aion::gameserver::controllers
