#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Kromede's Trial (300230000): the race's transformation on entry and revive; the movies, each once per instance; Kaliga's treasury spawns
 * him weakened when his three guards died first; the relics take his buffs and come back while he lives; the relic keys stay in the instance.
 * <p>
 * Java: data/handlers/instance/KromedesTrialInstance.java, @InstanceID(300230000). C++: the scheduled lambda is pinned to the handler.
 *
 * @author xTz, Gigi, Pad
 */
class KromedesTrialInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean isBossSpawned{AION_LOCK_CLASS(KromedesTrialInstance::isBossSpawned)}; // Java: = new AtomicBoolean()
	runtime::ArrayList<int32_t> sentMovies{AION_LOCK_CLASS(KromedesTrialInstance::sentMovies)}; // Java: = new ArrayList<>()
	runtime::Field<int32_t> skillId{};
	runtime::Field<bool> isPlayerInManor{};

protected:
	explicit KromedesTrialInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<KromedesTrialInstance> create(WorldMapInstance& instance);

	void onEnterInstance(Player& player) override;

	void onEnterZone(Player& player, ZoneInstance& zone) override;

	void onPlayMovieEnd(Player& player, int32_t movieId) override;

	void onDie(Npc& npc) override;

	bool onReviveEvent(Player& player) override;

private:
	void removeKaligaBuff(int32_t skillId);

	void scheduleRespawn(Npc& npc);

	bool isDead(runtime::Ptr<Npc> npc);

	void sendMovie(Player& player, int32_t movieId);

protected:
	bool isRestrictedToInstance(Item& item) override;
};

} // namespace aion::gameserver::handlers::instance
