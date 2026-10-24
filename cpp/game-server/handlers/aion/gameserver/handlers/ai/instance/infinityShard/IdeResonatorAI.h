#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::infinityShard {

/**
 * The AI of the Ide Resonators in Infinity Shard ("ide_resonator"): spawned, a resonator targets Hyperion (231073); 8 s later, both alive, it
 * casts its first buff (21381 for 231093, 21383 for 231094, 21257 otherwise), 10 s after that the first of Hyperion's charges he lacks (21258,
 * 21382, 21384, 21416); the end of a charge cast puts 21371 on the resonator. No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/infinityShard/IdeResonatorAI.java, @AIName("ide_resonator") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Luzien, Estrayl
 */
class IdeResonatorAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit IdeResonatorAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void cancelTask();

protected:
	void handleDied() override;

	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::infinityShard
