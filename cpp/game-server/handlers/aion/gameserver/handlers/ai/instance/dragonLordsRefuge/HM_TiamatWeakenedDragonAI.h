#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatWeakenedDragonAI.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the anguished weakened Tiamat ("hm_tiamat_weakened_dragon"): phases at 50, 25, 20, 15 and 5 %; the atrocity follows the
 * players' mean angle within 46 m; sinking sand from 1 minute every 50 s, denser as her hp drops; the gravity tornadoes on six spots
 * left and right (reset at the spawn and home); its own creations, pain and adds.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/HM_TiamatWeakenedDragonAI.java, @AIName("hm_tiamat_weakened_dragon") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class HM_TiamatWeakenedDragonAI : public TiamatWeakenedDragonAI {
private:
	runtime::ArrayList<int32_t> gravityTornadoDistanceLeft{AION_LOCK_CLASS(HM_TiamatWeakenedDragonAI::gravityTornadoDistanceLeft)};
	runtime::ArrayList<int32_t> gravityTornadoDistanceRight{AION_LOCK_CLASS(HM_TiamatWeakenedDragonAI::gravityTornadoDistanceRight)};

public:
	explicit HM_TiamatWeakenedDragonAI(Npc& owner);

	void handleHpPhase(int32_t phaseHpPercent) override;

	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void spawnAtrocity(int32_t heading);

protected:
	int32_t calculateAtrocitySkillId() override;

	void scheduleSinkingSand() override;

private:
	void spawnSinkingSand();

	void spawnSinkingSandOnRndPos();

protected:
	void scheduleDivisiveCreations(int32_t delay) override;

	void spawnGravityCrusher() override;

private:
	void spawnGravityTornadoByDistAndAngle(int32_t distance, int32_t angle);

protected:
	void spawnInfinitePain() override;

	void despawnAdds() override;

private:
	void addGravityTornadoSpots();

protected:
	void handleSpawned() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
