#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Tahabata's altars in Tiamat Stronghold ("tahabataaltar"): meant to debuff players in its band. No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/TahabataAltarAI.java, @AIName("tahabataaltar") (the marker is in the .cpp). **Java bug,
 * ported (proposed correction J-I6-5):** the debuff is chosen for 283116 (20970) and 283118 (20971) but the bands are checked for 283253 and
 * 283255, so an altar never debuffs (283253/283255 get skill 0); proposed: one set of ids for both (the floors 283116/283118).
 *
 * @author Cheatkiller
 */
class TahabataAltarAI : public NpcAI {
public:
	explicit TahabataAltarAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleCreatureMoved(Creature& creature) override;

private:
	void checkDistance(Creature& creature);

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
