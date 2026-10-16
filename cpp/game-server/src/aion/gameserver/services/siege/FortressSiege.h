#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {
class SM_SYSTEM_MESSAGE;
} // namespace aion::gameserver::network::aion::serverpackets

namespace aion::gameserver::services::siege {

/**
 * Java: services.siege.FortressSiege (a Siege<FortressLocation>): a fortress siege - the fortress turns vulnerable, its enemies are thrown out,
 * the siege npcs and the boss spawn; at the end the owner changes or keeps the fortress, the world buffs, the rewards and the outposts follow.
 * The location is the base's SiegeLocation, narrowed by getSiegeLocation() (Java's type parameter).
 *
 * @author Source
 */
class FortressSiege : public Siege {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<MercenaryLocation>> activeMercenaryLocs{AION_LOCK_CLASS(FortressSiege::activeMercenaryLocs#stripe)};
	runtime::Field<int32_t> oldLegionId{};

protected:
	explicit FortressSiege(model::siege::FortressLocation& fortress);

public:
	static runtime::Ref<FortressSiege> create(model::siege::FortressLocation& fortress);

	/** Java: Siege<FortressLocation>.getSiegeLocation() - the base's location as the FortressLocation it is */
	runtime::Ptr<model::siege::FortressLocation> getSiegeLocation() const;

	void onSiegeStart() override;

private:
	/** Handles an additional assault of race-specific troops (asmo/ely only), to ensure players glory point rewards */
	void spawnFactionTroopAssault(model::siege::SiegeRace race);

	void initMercenaryZones();

public:
	void onSiegeFinish() override;

private:
	void onCapture();

	network::aion::serverpackets::SM_SYSTEM_MESSAGE getWinnerMsg(model::siege::SiegeRace winnerRace, runtime::Ptr<model::team::legion::Legion> winnerLegion,
		const std::string& locationName);

	network::aion::serverpackets::SM_SYSTEM_MESSAGE getLoserMsg(model::siege::SiegeRace loserRace, runtime::Ptr<model::team::legion::Legion> oldLegion,
		const std::string& locationName);

	int32_t getFactionBalanceAdjustment();

	void onDefended();

	void applyWorldBuffs(model::siege::SiegeRace winner, bool isDefense);

	void distributeLegionRewards(SiegeRaceCounter& winnerRaceCounter);

	void distributeLegionGp(model::team::legion::Legion& legion, SiegeRaceCounter& src);

	void distributeLegionRewards(model::team::legion::Legion& legion);

public:
	bool isEndless() override;

	void onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abyssPoints) override;

protected:
	runtime::Ptr<model::siege::ArtifactLocation> getArtifact();

	bool hasArtifact();

public:
	runtime::Ptr<MercenaryLocation> getMercenaryLocationByZoneId(int32_t zoneId);

protected:
	~FortressSiege() override;
};

} // namespace aion::gameserver::services::siege
