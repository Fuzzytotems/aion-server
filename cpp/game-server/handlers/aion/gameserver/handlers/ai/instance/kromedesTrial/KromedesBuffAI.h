#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

/**
 * The AI of Kromede's Trial's buff objects ("krbuff"): the use gives the object's buff and its message; two of the four go away.
 * <p>
 * Java: data/handlers/ai/instance/kromedesTrial/KromedesBuffAI.java, @AIName("krbuff") (the marker is in the .cpp).
 *
 * @author Tiger0319, Gigi, xTz
 */
class KromedesBuffAI : public ActionItemNpcAI {
public:
	explicit KromedesBuffAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
