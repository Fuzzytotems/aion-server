#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an NPC that never acts ("noaction"): it does not fight back at all, and the training dummies among them drop their aggro again so
 * that their HP regenerates.
 * <p>
 * Java: data/handlers/ai/NoActionAI.java, @AIName("noaction") (the marker is in the .cpp).
 *
 * @author ATracer
 */
class NoActionAI : public NpcAI {
public:
	explicit NoActionAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;
};

} // namespace aion::gameserver::handlers::ai
