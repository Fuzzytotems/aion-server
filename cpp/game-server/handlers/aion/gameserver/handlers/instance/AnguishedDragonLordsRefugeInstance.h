#pragma once

#include "aion/gameserver/handlers/instance/DragonLordsRefugeInstance.h"

#include <cstdint>
#include <initializer_list>

namespace aion::gameserver::handlers::instance {

/**
 * Anguished Dragon Lord's Refuge (300630000): Dragon Lord's Refuge with the anguished npc ids, the spiritualists going as they die and its own
 * exit (833482). **Java bugs, ported (proposed corrections J-P6I13-1 and J-P6I13-2):** the empyrean lord's death makes 283163-283166 (the
 * normal refuge's spiritualists) cast 20983 instead of the incarnations 236278-236281, and the weakened Tiamat's death spares 219490, 219493
 * and 219362 (the normal refuge's ids) instead of 856022, 856025 and 236277.
 * <p>
 * Java: data/handlers/instance/AnguishedDragonLordsRefugeInstance.java, @InstanceID(300630000).
 *
 * @author Estrayl
 */
class AnguishedDragonLordsRefugeInstance : public DragonLordsRefugeInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit AnguishedDragonLordsRefugeInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<AnguishedDragonLordsRefugeInstance> create(WorldMapInstance& instance);

	void onSpawn(VisibleObject& object) override;

	void onStartEffect(runtime::Ptr<Effect> effect) override;

	void onEndEffect(Effect& effect) override;

private:
	void endInstance(std::initializer_list<int32_t> despawnExceptions);

public:
	using DragonLordsRefugeInstance::onDie;

	void onDie(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void handlePhaseProgress();

protected:
	void spawnTiamat() override;

	void spawnCalindi() override;
};

} // namespace aion::gameserver::handlers::instance
