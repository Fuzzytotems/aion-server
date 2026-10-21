#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Tahabata's altars in Tiamat Stronghold ("tahabataaltar"): meant to debuff players in its band. No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/TahabataAltarAI.java, @AIName("tahabataaltar") (the marker is in the .cpp). **Correction
 * of the Java code (owner's decision 2026-10-19, both branches; was J-I6-5):** the bands are checked on 283116 (25-37 m, 20970) and 283118
 * (20-25 m, 20971), the ids the debuff is chosen for; Java checked 283253 and 283255, ids no npc template has, so an altar never debuffed.
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
