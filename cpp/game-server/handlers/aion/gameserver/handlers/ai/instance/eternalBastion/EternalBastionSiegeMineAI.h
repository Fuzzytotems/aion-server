#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionConstructAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of the siege mines ("eternal_bastion_siege_mine"): the first attack arms them (21143); at its end the mine destroys itself and leaves
 * its explosion npc.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionSiegeMineAI.java, @AIName("eternal_bastion_siege_mine") (the marker is in the .cpp).
 *
 * @author ViAl, Estrayl
 */
class EternalBastionSiegeMineAI : public EternalBastionConstructAI {
private:
	runtime::AtomicBoolean isActivated{AION_LOCK_CLASS(EternalBastionSiegeMineAI::isActivated)}; // Java: = new AtomicBoolean()

public:
	explicit EternalBastionSiegeMineAI(Npc& owner) : EternalBastionConstructAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
