#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::padmarashkasCave {

/**
 * Padmarashka's eggs ("padmarashkaegg"): attacked, an egg calls a protector, whose wrath its death wakes; unbroken, it hatches after one
 * minute (small) or two (huge) and the hatchling goes for Padmarashka's target.
 * <p>
 * Java: data/handlers/ai/instance/padmarashkasCave/PadmarashkaEggAI.java, @AIName("padmarashkaegg") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class PadmarashkaEggAI : public NpcAI {
	runtime::Field<bool> isSmallEggProtectorSpawned{false};
	runtime::Field<bool> isHugeEggProtectorSpawned{false};

private:
	runtime::Field<runtime::Ref<Npc>> protector{};

public:
	explicit PadmarashkaEggAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDied() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void SpawnEliteCommander();

protected:
	void handleSpawned() override;

private:
	void smallEggSpawn();

	void hugeEggSpawn();

	void attackPlayer(Npc& npc);
};

} // namespace aion::gameserver::handlers::ai::instance::padmarashkasCave
