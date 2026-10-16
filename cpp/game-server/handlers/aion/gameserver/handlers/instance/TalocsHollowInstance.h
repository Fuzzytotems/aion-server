#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::instance {

/**
 * Taloc's Hollow (300190000): the entry gives the missing tears and fruit (not during the campaign quests); Queen Mosqua's door closes while
 * she fights; the bosses open the doors and break the insect egg (which releases Shishir's and Gellmar's summons); the two rings' movies,
 * once each; the healing objects; the revive at the entrance.
 * <p>
 * Java: data/handlers/instance/TalocsHollowInstance.java, @InstanceID(300190000).
 */
class TalocsHollowInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<int32_t> movies{AION_LOCK_CLASS(TalocsHollowInstance::movies)}; // Java: = new ArrayList<>()
	runtime::AtomicBoolean isQueenMosquaHome{AION_LOCK_CLASS(TalocsHollowInstance::isQueenMosquaHome), true};

protected:
	explicit TalocsHollowInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<TalocsHollowInstance> create(WorldMapInstance& instance);

	void onEnterInstance(Player& player) override;

private:
	void addItems(Player& player);

	void addMissingItems(Player& player, int32_t itemId);

protected:
	bool isRestrictedToInstance(Item& item) override;

public:
	void onAggro(Npc& npc) override;

	void onBackHome(Npc& npc) override;

	void onDie(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void sendMovie(Player& player, int32_t movie);

public:
	void onInstanceCreate() override;

private:
	void spawnRings();

public:
	bool onPassFlyingRing(Player& player, std::string_view flyingRing) override;

	bool onReviveEvent(Player& player) override;
};

} // namespace aion::gameserver::handlers::instance
