#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"

#include "aion/gameserver/runtime/collections/ArrayList.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {

/**
 * The AI of Stonespear Reach's attackers ("aggressive_stonespear"): spawned, they hate the first guardian stone of the instance they find
 * (3000) and fight; dead, they are deleted at once; no respawn or reward.
 * <p>
 * Java: data/handlers/ai/instance/stonespearReach/StonespearAggressiveNpcAI.java, @AIName("aggressive_stonespear").
 *
 * @author Yeats
 */
class StonespearAggressiveNpcAI : public AggressiveNoLootNpcAI {
private:
	runtime::ArrayList<int32_t> guardIds{AION_LOCK_CLASS(StonespearAggressiveNpcAI::guardIds)}; // Java: = new ArrayList<>()

public:
	explicit StonespearAggressiveNpcAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void findGuardianStone();

protected:
	void handleDied() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::stonespearReach
