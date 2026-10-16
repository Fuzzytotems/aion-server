#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::classNpc {

/**
 * The AI of the drakan priests' healing servants ("drakanhealingservant"): two seconds after the spawn it targets its creator and heals
 * it every 6 s; no decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/classNpc/DrakanHealingServantAI.java, @AIName("drakanhealingservant") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class DrakanHealingServantAI : public NpcAI {
public:
	explicit DrakanHealingServantAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void heal();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::classNpc
