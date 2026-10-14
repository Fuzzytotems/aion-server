#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/OneDmgNoActionAI.h"

namespace aion::gameserver::handlers::ai::instance::dredgion {

/**
 * recieve only 1 dmg with each attack(handled by super) Aggro the whole room on attack
 * <p>
 * Java: data/handlers/ai/instance/dredgion/SurkanaAI.java, @AIName("surkana").
 *
 * @author Luzien
 */
class SurkanaAI : public OneDmgNoActionAI {
public:
	explicit SurkanaAI(Npc& owner) : OneDmgNoActionAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void checkForSupport(Creature& creature);
};

} // namespace aion::gameserver::handlers::ai::instance::dredgion
