#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of Virhana the Great ("virhana"): attacked, she casts 19121, then from 70 s twelve rages (18897) 10 s apart, and over again.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/VirhanaTheGreatAI.java, @AIName("virhana") (the marker is in the .cpp).
 *
 * @author unknown
 */
class VirhanaTheGreatAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> isStart{};
	runtime::Field<int32_t> count{};

public:
	explicit VirhanaTheGreatAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void handleAttack(runtime::Ptr<Creature> creature) override;

protected:
	void handleBackHome() override;

private:
	void scheduleRage();

	void startRage();
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
