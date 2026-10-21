#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include "aion/gameserver/custom/instance/InstanceScoring.h"

namespace aion::gameserver::handlers::instance {

/**
 * Fire Temple (320100000): each named boss spawns at 75 %, an elite in its place otherwise; the stronger Kromede at 10 %.
 * <p>
 * Java: data/handlers/instance/FireTempleInstance.java.
 * <p>
 * C++-dev only: the Hard mode of the difficulty menu example (custom/difficulty/DifficultyMenu.h, docs/deviations/C2.md). An instance created by
 * the menu's Hard entry doubles every spawned npc's max HP and raises its attack by half, and tells each entering player so. With
 * RuntimeConfig::INSTANCE_SCORING_ENABLE it is also scored (custom/instance/InstanceScoring.h): a 20-minute countdown from the first entry,
 * points per kill, the end at Kromede's death or the time out.
 *
 * @author Gigi
 */
class FireTempleInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit FireTempleInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<FireTempleInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	/** C++-dev: Hard scales every npc spawned in this instance (applyHardMode) */
	void onSpawn(VisibleObject& object) override;

	/** C++-dev: Hard tells the entering player */
	void onEnterInstance(Player& player) override;

	/** C++-dev: 1 for the menu's Hard, 0 otherwise (Java's entry) */
	int8_t getDifficulty() const { return difficulty; }

	/** C++-dev: max HP x2, physical and magical attack x1.5 (StatSetFunctions, as the Eternal Challenge adapts its npcs) */
	static void applyHardMode(Npc& npc);

	static constexpr const char* HARD_MODE_MESSAGE = "Fire Temple - Hard: enemies have doubled health and stronger attacks.";

	/** C++-dev: the kill's points (Hard with scoring) */
	void onDie(Npc& npc) override;

	/** C++-dev: the scoring's timer task ends with the instance */
	void onInstanceDestroy() override;

	/** C++-dev: Fire Temple Hard's scoring settings - 20 minutes, the named bosses, Kromede as the final boss, the rewards per rank */
	static custom::instance::InstanceScoring::Settings scoringSettings();

	/** C++-dev: the score of a Hard instance with scoring on, else null */
	runtime::Ptr<custom::instance::InstanceScoring> getScoring() const { return scoring.get(); }

	/** C++-dev: the scoring's time out (the timer task's body) */
	void onScoringTimeout();

private:
	const int8_t difficulty;
	runtime::Field<runtime::Ref<custom::instance::InstanceScoring>> scoring{}; // C++-dev only
	runtime::Field<runtime::FutureRef> scoringTimeout{};                      // C++-dev only
};

} // namespace aion::gameserver::handlers::instance
