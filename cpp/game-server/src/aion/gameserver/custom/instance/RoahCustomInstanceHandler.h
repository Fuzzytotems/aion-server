#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/custom/instance/RoahDpsReport.h"
#include "aion/gameserver/custom/instance/fwd.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::custom::instance {

/**
 * The Eternal Challenge in the Roah chamber (D12, owner 2026-10-13): one player against the central Aether Stone and its protectors, then
 * against a boss driven by the player's own model.
 * <p>
 * C++: created by CustomInstanceService.onEnter through InstanceService's handler supplier (`create(instance)` stands for
 * `RoahCustomInstanceHandler::new`). The tasks' lambdas are pinned to the handler. Java's plain fields are Fields (the tasks write them).
 *
 * @author Jo, Estrayl
 */
class RoahCustomInstanceHandler : public gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t CENTER_ARTIFACT_ID = 234029;
	static constexpr int32_t TRASH_MOB_ID = 218483;
	static constexpr int32_t BULKY_MOB_ID = 282757;
	static constexpr int32_t DOMINATOR_MOB_ID = 284203;
	static constexpr int32_t BOSS_MOB_A_M_ID = 231919; // asmodian male
	static constexpr int32_t BOSS_MOB_A_F_ID = 231747; // asmodian female
	static constexpr int32_t BOSS_MOB_E_M_ID = 231918; // elyos male
	static constexpr int32_t BOSS_MOB_E_F_ID = 231717; // elyos female
	static constexpr int32_t BOSS_MOB_AT_ID = 217221;
	static constexpr int32_t TIME_LIMIT = 900; // 15 minutes
	static constexpr float REWARD_SCALE = 2.0f;
	runtime::AtomicLong startTime{AION_LOCK_CLASS(RoahCustomInstanceHandler::startTime)}; // Java: = new AtomicLong()
	runtime::AtomicBoolean isInitialized{AION_LOCK_CLASS(RoahCustomInstanceHandler::isInitialized)}; // Java: = new AtomicBoolean()
	runtime::AtomicBoolean isCompleted{AION_LOCK_CLASS(RoahCustomInstanceHandler::isCompleted)}; // Java: = new AtomicBoolean()
	runtime::Field<int32_t> avgClassDps{};
	runtime::Field<int32_t> achievedDps{};
	runtime::Field<int32_t> playerObjId{};
	runtime::Field<runtime::Ref<neuralnetwork::PlayerModel>> model{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<int32_t>>> skillSet{};
	runtime::Field<bool> isBossPhase{};
	runtime::Field<bool> isPrebuffed{};
	runtime::Field<int32_t> rank{};
	runtime::Field<runtime::FutureRef> despawnTask{};
	runtime::Field<runtime::FutureRef> trashMobSpawnTask{};
	runtime::Field<runtime::FutureRef> bulkyMobSpawnTask{};
	runtime::Field<runtime::FutureRef> dominatorMobSpawnTask{};
	// C++-dev only, not in Java: the DPS report (docs/deviations/P5-13.md, "C++-dev only: the Eternal Challenge DPS report"), a part; the wall
	// clock, as startTime
	const std::unique_ptr<RoahDpsReport> dpsReport;
	runtime::AtomicBoolean runDamageClosed{AION_LOCK_CLASS(RoahCustomInstanceHandler::runDamageClosed)}; // C++-dev only: alive npcs counted once
protected:
	explicit RoahCustomInstanceHandler(world::WorldMapInstance& instance);
public:
	static runtime::Ref<RoahCustomInstanceHandler> create(world::WorldMapInstance& instance);
	void onInstanceCreate() override;
	void onInstanceDestroy() override;
private:
	void spawnRings();
public:
	bool onPassFlyingRing(gameserver::model::gameobjects::player::Player& player, std::string_view flyingRing) override;
	void onDie(gameserver::model::gameobjects::Npc& npc) override;
	void onEnterInstance(gameserver::model::gameobjects::player::Player& player) override;
	/** C++-dev only, not in Java: a run left before its end gets its failure report */
	void onLeaveInstance(gameserver::model::gameobjects::player::Player& player) override;
private:
	bool checkForPrebuffOnEntry(gameserver::model::gameobjects::player::Player& player);
	void setAverageDpsByPlayerClass(gameserver::model::PlayerClass playerClass);
	void calcBossDrop(int32_t npcObjId);
	int32_t getRewardCoinAmount(int32_t rank);
	void adaptNPC(gameserver::model::gameobjects::Npc& npc, int32_t rank);
	void setResult(bool success);
	/** C++-dev only: setResult with the reason a failed run gives in the DPS report */
	void setResult(bool success, std::string_view failureReason);
	/** C++-dev only: sends a DPS report line to the player's chat log (GOLDEN_YELLOW), when he is in the instance */
	void sendReport(const std::string& text);
	/** C++-dev only: the damage the player (with his summons and servants) dealt to the npc, from its aggro list */
	int64_t playerDamageOn(gameserver::model::gameobjects::Npc& npc);
	/** C++-dev only: adds the player's damage on every living npc of the instance but `except` to the report's current phase */
	void addDamageOnAliveNpcs(const gameserver::model::gameobjects::Npc* except);
	/** C++-dev only: addDamageOnAliveNpcs once, when the run ends (the boss's death, a failure or leaving) */
	void closeRunDamage(const gameserver::model::gameobjects::Npc* except);
	void cancelAllTasks();
	void spawnUnlockableNpcs(gameserver::model::gameobjects::player::Player& player);
	int32_t getElapsedTimeMillis();
public:
	bool onReviveEvent(gameserver::model::gameobjects::player::Player& player) override;
	bool onDie(gameserver::model::gameobjects::player::Player& player, gameserver::model::gameobjects::Creature& lastAttacker) override;
	void onEndCastSkill(skillengine::model::Skill& skill) override;
	runtime::Ptr<neuralnetwork::PlayerModel> getPlayerModel() const { return this->model.get(); }
	/** Java: the list itself (null before the player entered) */
	runtime::Ptr<runtime::RcArrayList<int32_t>> getSkillSet() const { return this->skillSet.get(); }
	/** C++-dev only: the DPS report (tests) */
	const RoahDpsReport& getDpsReport() const { return *dpsReport; }
	bool allowSelfReviveBySkill() override;
	bool allowSelfReviveByItem() override;
	bool allowInstanceRevive() override;
protected:
	~RoahCustomInstanceHandler() override;
};

} // namespace aion::gameserver::custom::instance
