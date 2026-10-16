#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of the magic artifact ("magicartifact"): attacked, it casts 18916, at most once a second.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/MagicArtifactAI.java, @AIName("magicartifact") (the marker is in the .cpp).
 *
 * @author unknown
 */
class MagicArtifactAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> cooldown{false};

public:
	explicit MagicArtifactAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void setCD();
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
