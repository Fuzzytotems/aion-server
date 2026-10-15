#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of Xasta's walk in the Rentus Bases ("idyun_hard_xasta"): it does not think; at the end of its walk it tells the instance
 * handler (onSpecialEvent) once.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/IdyunHardXasta.java, @AIName("idyun_hard_xasta") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class IdyunHardXasta : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isDestinationReached{AION_LOCK_CLASS(IdyunHardXasta::isDestinationReached), false}; // Java: = new AtomicBoolean(false)

public:
	explicit IdyunHardXasta(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleMoveArrived() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
