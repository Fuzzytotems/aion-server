#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/siege/fwd.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/siege/Siege.h"

namespace aion::gameserver::services::siege {

/**
 * Java: services.siege.AgentSiege (a Siege<AgentLocation>): the agent fight of Levinshor (8011) - announced, then after nine 60 s steps the
 * agents Veille and Mastarius spawn; the race whose agent wins (setWinnerRace, set by the agent AIs) captures base 6113. The location is the
 * base's SiegeLocation, narrowed by getSiegeLocation() (Java's type parameter).
 *
 * @author Estrayl, Sykra
 */
class AgentSiege : public Siege {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int8_t> startProgress{1}; // Java: = 1
	runtime::Field<runtime::Ref<model::gameobjects::siege::SiegeNpc>> masta{};
	runtime::Field<runtime::Ref<model::gameobjects::siege::SiegeNpc>> veille{};
	// fieldmap.toml: null until setWinnerRace (onSiegeFinish returns on null, AgentSiege.java:71)
	runtime::Field<std::optional<model::siege::SiegeRace>> winner{};

protected:
	explicit AgentSiege(model::siege::AgentLocation& siegeLocation);

public:
	static runtime::Ref<AgentSiege> create(model::siege::AgentLocation& siegeLocation);

	/** Java: Siege<AgentLocation>.getSiegeLocation() - the base's location as the AgentLocation it is */
	runtime::Ptr<model::siege::AgentLocation> getSiegeLocation() const;

protected:
	void onSiegeStart() override;

private:
	void delayStart();

protected:
	void onSiegeFinish() override;

private:
	void broadcastAgentSpawn();

	void distributeQuest();

public:
	void spawnSiegeNpcs();

	void despawnSiegeNpcs();

private:
	void initNpc(model::gameobjects::siege::SiegeNpc& target);

public:
	void setWinnerRace(model::siege::SiegeRace race);

	bool isEndless() override;

	void onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abyssPoints) override;

	/** C++ addition for tests: Java's private startProgress */
	int8_t getStartProgressForTests() const { return startProgress.get(); }

protected:
	~AgentSiege() override;
};

} // namespace aion::gameserver::services::siege
