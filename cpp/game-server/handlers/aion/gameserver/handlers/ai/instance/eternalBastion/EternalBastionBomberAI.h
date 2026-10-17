#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Eternal Bastion's bombers ("eternal_bastion_bomber"): they run their route; the first attack arms the explosion (21143), whose end
 * destroys the bomber and drops two gate-breaking charges.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionBomberAI.java, @AIName("eternal_bastion_bomber") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Estrayl
 */
class EternalBastionBomberAI : public EternalBastionAggressiveNpcAI {
private:
	runtime::AtomicBoolean isExplosionActivated{AION_LOCK_CLASS(EternalBastionBomberAI::isExplosionActivated)}; // Java: = new AtomicBoolean()

public:
	explicit EternalBastionBomberAI(Npc& owner) : EternalBastionAggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void startWalking();

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
