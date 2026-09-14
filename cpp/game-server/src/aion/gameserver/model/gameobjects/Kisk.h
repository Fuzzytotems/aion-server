#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/stats/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A visible object: `VisibleObject::create<Kisk>(controller, spawnTemplate, owner)`
 * (§10.1). Java `SummonedObject<Player>`: the erased base returns the creator as `Ptr<VisibleObject>` (§8.1); Kisk passes null as creator and
 * keeps the owner's id and name instead. The constructor reads the NPC template (DataManager) and creates the known list and effect controller,
 * so it stays `AION_UNPORTED` (the SummonedObject base initializer already is).
 *
 * @author Sarynth, nrg
 */
class Kisk : public SummonedObject {
	AION_MAKE_REF_FRIEND
private:
	static inline const int64_t KISK_LIFETIME_IN_SEC = 2 * 60 * 60; // Java: TimeUnit.HOURS.toSeconds(2)
	const int32_t legionId;
	const Race ownerRace;
	const templates::stats::KiskStatsTemplate* kiskStatsTemplate;
	runtime::Field<int32_t> remainingResurrections;
	runtime::ConcurrentKeySet<int32_t> kiskMemberIds{AION_LOCK_CLASS(Kisk::kiskMemberIds#stripe)};

protected:
	Kisk(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, player::Player& owner);
	~Kisk() override;

public:
	bool isEnemy(Creature& creature) override;

	/** C++: keeps the other isEnemyFrom overloads visible next to the override (no hiding in Java) */
	using SummonedObject::isEnemyFrom;

	bool isEnemyFrom(player::Player& player) override;

	CreatureType getType(Creature& creature) override;

	/** @return NpcObjectType.NORMAL */
	NpcObjectType getNpcObjectType() override;

	/** @return Returns the useMask. */
	int32_t getUseMask();

	std::vector<runtime::Ptr<player::Player>> getCurrentMemberList();

	/** @return current member count */
	int32_t getCurrentMemberCount();

	runtime::ConcurrentKeySet<int32_t>& getCurrentMemberIds() { return kiskMemberIds; }

	/** @return max member count */
	int32_t getMaxMembers();

	/** @return remaining resurrections */
	int32_t getRemainingResurrects() const { return remainingResurrections.get(); }

	/** @return max resurrections */
	int32_t getMaxRessurects();

	/** @return remaining lifetime in seconds. */
	int32_t getRemainingLifetime();

	/** @return True if player is able to bind to this kisk */
	bool canBind(player::Player& player);

private:
	bool isUseAllowed(player::Player& player);

public:
	void addPlayer(player::Player& player);

	void removePlayer(player::Player& player);

private:
	/** Sends SM_KISK_UPDATE to each member */
	void broadcastKiskUpdate();

public:
	void broadcastPacket(network::aion::serverpackets::SM_SYSTEM_MESSAGE& message);

	void resurrectionUsed();

	Race getOwnerRace() const { return ownerRace; }

	bool isActive();
};

} // namespace aion::gameserver::model::gameobjects
