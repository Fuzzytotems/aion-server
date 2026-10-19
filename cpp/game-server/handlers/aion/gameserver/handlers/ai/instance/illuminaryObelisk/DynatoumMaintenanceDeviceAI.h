#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

/**
 * Dynatoum's maintenance devices ("dynatoum_maintenance_device"): no thinking; they target Dynatoum and heal it every 10 s.
 * <p>
 * Java: data/handlers/ai/instance/illuminaryObelisk/DynatoumMaintenanceDeviceAI.java, @AIName("dynatoum_maintenance_device") (the marker is in the .cpp).
 *
 * @author M.O.G. Dision, Estrayl
 */
class DynatoumMaintenanceDeviceAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit DynatoumMaintenanceDeviceAI(Npc& owner) : NpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

private:
	void scheduleSkillTask();

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
