#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance::crucible {

/**
 * The base of the two crucibles (Crucible Challenge, Empyrean Crucible): a CruciblePlayerReward per player who enters, the stage, the
 * revive in place at full hp and mp, and no self revive.
 * <p>
 * Java: data/handlers/instance/crucible/CrucibleInstance.java (no @InstanceID: not registered).
 *
 * @author xTz
 */
class CrucibleInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	runtime::Field<bool> isInstanceDestroyed{false};
	runtime::Field<StageType> stageType{StageType::DEFAULT};
	runtime::Field<runtime::Ref<InstanceScore>> instanceScore{}; // Java: InstanceScore<CruciblePlayerReward> (erased)

	explicit CrucibleInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<CrucibleInstance> create(WorldMapInstance& instance);

	void onEnterInstance(Player& player) override;

	void onInstanceCreate() override;

private:
	void addPlayerReward(Player& player);

protected:
	runtime::Ptr<CruciblePlayerReward> getPlayerReward(int32_t objectId);

public:
	runtime::Ptr<InstanceScore> getInstanceScore() override;

protected:
	/** Java's null after the instance is destroyed is std::nullopt */
	std::optional<std::vector<runtime::Ptr<Npc>>> getNpcs(int32_t npcId);

	bool isInZone(const ZoneName* zone, Player& player);

	void teleport(Player& player, float x, float y, float z, int8_t h);

public:
	StageType getStage() override;

	bool onReviveEvent(Player& player) override;

	void onInstanceDestroy() override;

	bool allowSelfReviveBySkill() override;

	bool allowSelfReviveByItem() override;
};

} // namespace aion::gameserver::handlers::instance::crucible
