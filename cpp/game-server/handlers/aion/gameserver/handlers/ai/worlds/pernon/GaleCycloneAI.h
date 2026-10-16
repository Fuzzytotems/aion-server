#pragma once

#include <cstdint>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::worlds::pernon {

struct GaleCycloneAI_GaleCycloneObserver;

/**
 * The AI of a gale cyclone in Pernon ("gale_cyclone"): every player it sees gets a move observer, which casts 20528 on the player each time
 * the player comes within 12 m (from farther away), and removes itself when the cyclone is dead or about to die or no longer sees the
 * player. The cyclone's death or despawn removes all of them.
 * <p>
 * Java: data/handlers/ai/worlds/pernon/GaleCycloneAI.java, @AIName("gale_cyclone") (the marker is in the .cpp).
 * <p>
 * C++: Java's private inner class GaleCycloneObserver is the struct GaleCycloneAI_GaleCycloneObserver of the .cpp, which reads the map as a
 * friend, as the inner class reads its enclosing instance. The map holds the observers and an observer holds the AI: the cycle ends when the
 * observer is removed (its onRemoved), which the cyclone's death and despawn do for all of them.
 *
 * @author xTz
 */
class GaleCycloneAI : public NpcAI {
	friend struct GaleCycloneAI_GaleCycloneObserver;

private:
	/** Java: private final Map<Integer, GaleCycloneObserver> observed = new ConcurrentHashMap<>() */
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<GaleCycloneAI_GaleCycloneObserver>> observed{AION_LOCK_CLASS(GaleCycloneAI::observed#stripe)};

public:
	// C++: out of line, where the map's observer type is complete
	explicit GaleCycloneAI(Npc& owner);
	~GaleCycloneAI() override;

protected:
	void handleCreatureSee(Creature& creature) override;

	void handleDied() override;

	void handleDespawned() override;

private:
	void clear();
};

} // namespace aion::gameserver::handlers::ai::worlds::pernon
