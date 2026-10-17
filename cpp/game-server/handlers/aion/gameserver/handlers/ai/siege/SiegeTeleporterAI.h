#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A fortress teleporter ("siege_teleporter"): while it stands, its siege location can be teleported to; its spawn, death and despawn set the
 * location's flag and tell every player of the map with SM_FORTRESS_INFO.
 * <p>
 * Java: data/handlers/ai/siege/SiegeTeleporterAI.java, @AIName("siege_teleporter") (the marker is in the .cpp).
 *
 * @author Source
 */
class SiegeTeleporterAI : public GeneralNpcAI {
public:
	explicit SiegeTeleporterAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDespawned() override;

	void handleDied() override;

	void handleSpawned() override;

private:
	void canTeleport(bool status);
};

} // namespace aion::gameserver::handlers::ai::siege
