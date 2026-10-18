#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the incarnations' summons ("tiamats_incarnation_spawn"): from 2.5 s every 3 s its skill (by npc id); no decay or
 * rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/TiamatsIncarnationSpawnsAI.java, @AIName("tiamats_incarnation_spawn") (the marker is in
 * the .cpp).
 *
 * @author Estrayl
 */
class TiamatsIncarnationSpawnsAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit TiamatsIncarnationSpawnsAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDespawned() override;

private:
	int32_t getSkillId();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
