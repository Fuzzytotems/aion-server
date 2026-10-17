#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of the divine artifact ("divineartifact"): attacked, it casts 18915, at most once a second.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/DivineArtifactAI.java, @AIName("divineartifact") (the marker is in the .cpp).
 *
 * @author unknown
 */
class DivineArtifactAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> cooldown{false};

public:
	explicit DivineArtifactAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void setCD();
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
