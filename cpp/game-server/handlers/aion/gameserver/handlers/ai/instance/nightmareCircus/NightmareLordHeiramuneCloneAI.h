#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of Nightmare Lord Heiramune's clones ("nightmarelordheiramuneclone"): every 10 s from 5 s a clone heals Heiramune while he lives,
 * else it goes; the task is the owner's SKILL_USE task.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/NightmareLordHeiramuneCloneAI.java, @AIName("nightmarelordheiramuneclone")
 * (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class NightmareLordHeiramuneCloneAI : public AggressiveNpcAI {
public:
	explicit NightmareLordHeiramuneCloneAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void heal();
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
