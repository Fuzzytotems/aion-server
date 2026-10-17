#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Bosses.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

/**
 * The AI of Watchman Hokuruki in The Shugo Emperor's Vault ("watchman_hokuruki"): at 100 % and 15 % HP four 235632 come 3-5 m around him and
 * take his most hated player; at 75, 50 and 25 % a marksman and two snipers come at the three places in a random order (shuffled again back
 * home). His death deletes him.
 * <p>
 * Java: data/handlers/ai/instance/theShugoEmperorsVault/WatchmanHokuruki.java, @AIName("watchman_hokuruki") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class WatchmanHokuruki : public IDSweep_Bosses, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(100, {75, 50, 25, 15}); // Java: = new HpPhases(100, 75, 50, 25, 15)
	/** Java: Collections.synchronizedList(Arrays.asList(1, 2, 3)) */
	runtime::ArrayList<int32_t> randomPositions{AION_LOCK_CLASS(WatchmanHokuruki::randomPositions)};

public:
	explicit WatchmanHokuruki(Npc& owner);

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void spawnAdds(int32_t position);

protected:
	void handleBackHome() override;

	void handleDied() override;

private:
	void rndSpawn(int32_t npcId, int32_t count);
};

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
