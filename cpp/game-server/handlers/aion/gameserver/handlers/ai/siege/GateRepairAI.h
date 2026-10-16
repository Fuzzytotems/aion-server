#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A fortress gate's repair stone ("siege_gaterepair"): talking to it asks STR_ASK_DOOR_REPAIR_POPUPDIALOG (with the remaining cooldown), then
 * STR_ASK_DOOR_REPAIR_DO_YOU_ACCEPT_REPAIR with the fee. A member of the owning race (and of the owning legion with the ARTIFACT right, when a
 * legion holds the fortress) who pays the fee heals the stone's door by gameserver.siege.door.repair.heal.percent; then the cooldown runs.
 * <p>
 * Java: data/handlers/ai/siege/GateRepairAI.java, @AIName("siege_gaterepair") (the marker is in the .cpp).
 *
 * @author Source
 */
class GateRepairAI : public NpcAI {
private:
	/** Java: LoggerFactory.getLogger("SIEGE_LOG") */
	static const commons::logging::Logger log;
	runtime::AtomicLong nextActivationTime{AION_LOCK_CLASS(GateRepairAI::nextActivationTime), 0}; // Java: = new AtomicLong(0)

public:
	explicit GateRepairAI(Npc& owner) : NpcAI(owner) {}

protected:
	/** Java's covariant override of getSpawnTemplate: the spawn narrowed to a SiegeSpawnTemplate (ClassCastException otherwise) */
	runtime::Ptr<SiegeSpawnTemplate> getSpawnTemplate();

	void handleDialogStart(Player& player) override;

	void handleDialogFinish(Player& player) override;

public:
	void onActivate(Player& player, const model::templates::siegelocation::DoorRepairData& repairData);

private:
	int32_t getCooldown();

	bool canActivate(Player& player, const model::templates::siegelocation::DoorRepairData& repairData);
};

} // namespace aion::gameserver::handlers::ai::siege
