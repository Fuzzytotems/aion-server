#pragma once

#include <optional>
#include <string_view>

#include "aion/gameserver/handlers/instance/InstancePrelude.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::handlers::instance {

/**
 * The Shugo Emperor's Vault (301400000), a scored 8-minute run: the three morph Shugos wait at the start; opening door 430 (or 60 s) starts the
 * run with the first room. Each kill scores (180 to 224,000 points, told to the map); Ruthless Jabaraki opens door 431 and the second room, two
 * Shugo Turncoats door 428 and Watchman Hokuruki's room; his death brings the vault door, the healing springs and the fourth room (respawning
 * every 90 s), after which the points call the five bosses in turn (50,000 / 82,500 / 120,000 / 162,000 / 275,000). Grand Commander Gradi's
 * death or the 8 minutes end the run: the rank (1-5 or 8) by the points, Vault keys (185000222) for ranks 1-4, the butler and the chests for
 * ranks 1-5. The morph effects (21829-21834) are kept and put back on re-entry; a revive brings the player back to the start.
 * <p>
 * Java: data/handlers/instance/TheShugoEmperorsVault.java. Java's synchronized checkStage, addPoints and checkRank use the monitor member.
 *
 * @author Yeats
 */
class TheShugoEmperorsVault : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	/** save transformations to avoid effect loss during sendlogs (Java: Map<Integer, Integer> = new ConcurrentHashMap<>()) */
	runtime::ConcurrentHashMap<int32_t, int32_t> transformationCache{AION_LOCK_CLASS(TheShugoEmperorsVault::transformationCache#stripe)};
	runtime::AtomicBoolean started{AION_LOCK_CLASS(TheShugoEmperorsVault::started)}; // Java: = new AtomicBoolean()
	runtime::AtomicInteger killedShugoTurncoats{AION_LOCK_CLASS(TheShugoEmperorsVault::killedShugoTurncoats)}; // Java: = new AtomicInteger()
	runtime::AtomicInteger stage{AION_LOCK_CLASS(TheShugoEmperorsVault::stage)}; // Java: = new AtomicInteger()
	runtime::ConcurrentKeySet<int32_t> spawns{AION_LOCK_CLASS(TheShugoEmperorsVault::spawns#stripe)}; // Java: = ConcurrentHashMap.newKeySet()
	runtime::Field<runtime::Ref<NormalScore>> instanceReward{};
	runtime::Field<runtime::FutureRef> timer{};
	runtime::Field<runtime::FutureRef> failTimerTask{};
	runtime::Field<int64_t> startTime{};
	runtime::Field<bool> lastStage{};
	runtime::Monitor monitor{AION_LOCK_CLASS(TheShugoEmperorsVault::monitor)}; // fieldmap: Java's synchronized methods

protected:
	explicit TheShugoEmperorsVault(WorldMapInstance& instance);

public:
	static runtime::Ref<TheShugoEmperorsVault> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onOpenDoor(int32_t door) override;

	void onStartEffect(runtime::Ptr<Effect> effect) override;

private:
	void startFailTask();

	void startInstance();

public:
	void onEnterInstance(Player& player) override;

	void onInstanceDestroy() override;

private:
	void reApplyTransformation(Player& player);

	void spawnRoom1();

	void spawnRoom2();

	void spawnRoom3();

	void spawnRoom4();

public:
	void onDie(Npc& npc) override;

private:
	void checkStage(int32_t points);

	void addPoints(Npc& npc, int32_t points);

	void spawnCaptainMirez();

	void spawnLongknifeZodica();

	void spawnSorcererBudyn();

	void spawnEliteCaptainRupasha();

	void spawnCommanderGradi();

	void spawnVaultDoorAndHealingSpring();

	void checkRank(int32_t totalPoints);

	void reward();

	void spawnShugos();

	void spawnMorphShugos();

	void spawnChests();

	void despawnAll();

	/** npcL10n: std::nullopt for Java's null */
	void sendPacket(std::optional<std::string_view> npcL10n, int32_t points);

	int32_t getTime();

public:
	runtime::Ptr<InstanceScore> getInstanceScore() override;

	bool onReviveEvent(Player& player) override;
};

} // namespace aion::gameserver::handlers::instance
