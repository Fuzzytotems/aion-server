#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/InstanceScoreType.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * @author xTz
 * <p>
 * C++: RefCounted like its base (create()). The erased base holds the score as InstanceScore; score() is the typed field of Java's generic
 * InstanceScoreWriter (the constructor takes the score class).
 */
class HarmonyScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND
private:
	const model::instance::InstanceScoreType type;
	const int32_t playerObjId;

protected:
	HarmonyScoreWriter(model::instance::instancescore::HarmonyArenaScore& reward, model::instance::InstanceScoreType type,
	                   model::gameobjects::player::Player& owner);
	HarmonyScoreWriter(model::instance::instancescore::HarmonyArenaScore& reward, model::instance::InstanceScoreType type);
	~HarmonyScoreWriter() override;

public:
	/** Java: new HarmonyScoreWriter(...) */
	static runtime::Ref<HarmonyScoreWriter> create(model::instance::instancescore::HarmonyArenaScore& reward, model::instance::InstanceScoreType type,
	                                               model::gameobjects::player::Player& owner);

	/** Java: new HarmonyScoreWriter(...) */
	static runtime::Ref<HarmonyScoreWriter> create(model::instance::instancescore::HarmonyArenaScore& reward, model::instance::InstanceScoreType type);

	void writeMe(commons::utils::ByteBuffer& buf) override;

private:
	/** Java: the typed field instanceScore (InstanceScoreWriter<HarmonyArenaScore>) */
	model::instance::instancescore::HarmonyArenaScore& score() const;

	void writePoints(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem);

	void writeReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem);

	/** @param rewardItem may be null */
	void writeSimpleReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::RewardItem* rewardItem);
};

} // namespace aion::gameserver::network::aion::instanceinfo
