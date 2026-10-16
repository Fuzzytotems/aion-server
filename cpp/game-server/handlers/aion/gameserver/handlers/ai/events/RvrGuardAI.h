#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the Silentera RvR event guards ("rvr_guard"): a player attacking it in Silentera Canyon from 19:00 to 23:59 while both
 * bosses stand joins the event's reward list.
 * <p>
 * Java: data/handlers/ai/events/RvrGuardAI.java, @AIName("rvr_guard") (the marker is in the .cpp).
 *
 * @author Bobobear
 */
class RvrGuardAI : public AggressiveNpcAI {
public:
	explicit RvrGuardAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;
};

} // namespace aion::gameserver::handlers::ai::events
