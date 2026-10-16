#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of the sacrificial souls ("templeSoul"): spawned, it casts 18901 and follows the living Isbariya (216263); arrived, it gives her
 * 18960 and goes; hit by a player with 18959 it goes; it never thinks.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/SacrificialSoulAI.java, @AIName("templeSoul") (the marker is in the .cpp).
 *
 * @author unknown
 */
class SacrificialSoulAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::Ref<Npc>> boss{}; // Java: Npc boss (null without a living Isbariya)

public:
	explicit SacrificialSoulAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleMoveArrived() override;

public:
	bool canThink() override;
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
