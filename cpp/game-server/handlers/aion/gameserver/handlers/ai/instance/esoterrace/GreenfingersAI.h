#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

/**
 * The AI of a greenfinger in Esoterrace ("greenfingers", Dalia Charlands's helpers): it walks its route without thinking; at its step it
 * stops, casts its helper skill on the boss 217185 and goes 3 seconds later.
 * <p>
 * Java: data/handlers/ai/instance/esoterrace/GreenfingersAI.java, @AIName("greenfingers"). C++: the despawn task is pinned to this AI.
 *
 * @author xTz
 */
class GreenfingersAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isDestroyed{AION_LOCK_CLASS(GreenfingersAI::isDestroyed)}; // Java: = new AtomicBoolean()
	runtime::Field<int32_t> walkPosition{};
	runtime::Field<int32_t> helperSkill{};

public:
	explicit GreenfingersAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;

private:
	void startDespawnTask();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
