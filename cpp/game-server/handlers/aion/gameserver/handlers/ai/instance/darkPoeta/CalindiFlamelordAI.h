#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * Calindi Flamelord, Dark Poeta's A-rank boss ("calindi_flamelord"): ten minutes, then "You are unworthy."; her fire skill calls
 * flames.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/CalindiFlamelordAI.java, @AIName("calindi_flamelord") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class CalindiFlamelordAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> wipeTask{};

public:
	explicit CalindiFlamelordAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void scheduleWipe();

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
