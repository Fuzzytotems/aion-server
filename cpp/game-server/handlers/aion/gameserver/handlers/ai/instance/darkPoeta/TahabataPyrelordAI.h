#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * Tahabata Pyrelord, Dark Poeta's S-rank boss ("tahabata_pyrelord"): five minutes, then "You are unworthy."; two skills call his adds.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/TahabataPyrelordAI.java, @AIName("tahabata_pyrelord") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class TahabataPyrelordAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> wipeTask{};

public:
	explicit TahabataPyrelordAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void scheduleWipe();

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void cancelTask();

protected:
	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
