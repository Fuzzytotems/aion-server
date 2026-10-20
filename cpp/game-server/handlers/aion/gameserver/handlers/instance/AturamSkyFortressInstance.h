#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Aturam Sky Fortress (300240000): the generators that weaken Hugen, the officers and chiefs whose deaths open the doors and send the
 * walking patrols, Marbata, the doping objects and the side windows.
 * <p>
 * Java: data/handlers/instance/AturamSkyFortressInstance.java, @InstanceID(300240000). C++: the scheduled lambdas are pinned to the handler.
 *
 * @author xTz
 */
class AturamSkyFortressInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean msgIsSent{AION_LOCK_CLASS(AturamSkyFortressInstance::msgIsSent)};
	runtime::AtomicInteger officerKilled{AION_LOCK_CLASS(AturamSkyFortressInstance::officerKilled)};
	runtime::AtomicInteger chiefKilled{AION_LOCK_CLASS(AturamSkyFortressInstance::chiefKilled)};
	runtime::AtomicInteger generators{AION_LOCK_CLASS(AturamSkyFortressInstance::generators)};
	runtime::Field<bool> isInstanceDestroyed{};

protected:
	explicit AturamSkyFortressInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<AturamSkyFortressInstance> create(WorldMapInstance& instance);

	using GeneralInstanceHandler::onDie;

	void onDie(Npc& npc) override;

private:
	void startMarbataWalkerEvent();

	void startWalk(Npc& npc, std::string_view walkId);

	void startOfficerWalkerEvent();

public:
	void onInstanceCreate() override;

	void onInstanceDestroy() override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	void onEnterZone(Player& player, ZoneInstance& zone) override;

	void onPlayMovieEnd(Player& player, int32_t movieId) override;
};

} // namespace aion::gameserver::handlers::instance
