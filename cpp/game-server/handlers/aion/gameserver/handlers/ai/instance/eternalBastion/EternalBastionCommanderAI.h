#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Eternal Bastion's fortress commanders ("eternal_bastion_commander"): out of combat they restore their hp after two minutes; they hate
 * Pashid at sight; attacks are announced to the map (the first, then 1 %).
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionCommanderAI.java, @AIName("eternal_bastion_commander") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EternalBastionCommanderAI : public EternalBastionAggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> hpRestoreTask{};
	runtime::Field<bool> wasAttacked{};

public:
	explicit EternalBastionCommanderAI(Npc& owner) : EternalBastionAggressiveNpcAI(owner) {}

	void handleFinishAttack() override;

private:
	void cancelTask();

protected:
	void handleCreatureAggro(Creature& creature) override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleBackHome() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
