#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * @author Neonm, Estrayl
 * <p>
 * C++: RefCounted like its base (create()). The erased base holds the score as InstanceScore; score() is the typed field of Java's generic
 * InstanceScoreWriter (the constructor takes the score class).
 */
class ArenaScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND
protected:
	const int32_t ownerObjectId;

private:
	const bool rewardTable;

protected:
	ArenaScoreWriter(model::instance::instancescore::PvPArenaScore& score, int32_t ownerObjectId, bool rewardTable);
	~ArenaScoreWriter() override;

public:
	/** Java: new ArenaScoreWriter(...) */
	static runtime::Ref<ArenaScoreWriter> create(model::instance::instancescore::PvPArenaScore& score, int32_t ownerObjectId, bool rewardTable);

	void writeMe(commons::utils::ByteBuffer& buf) override;

protected:
	void writePlayerScores(commons::utils::ByteBuffer& buf);

	/** Java: the typed field instanceScore (InstanceScoreWriter<PvPArenaScore>) */
	model::instance::instancescore::PvPArenaScore& score() const;

private:
	void writeOwnerRewards(commons::utils::ByteBuffer& buf);

	void writeReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem);

	/** @param rewardItem may be null */
	void writeSimpleReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::RewardItem* rewardItem);
};

} // namespace aion::gameserver::network::aion::instanceinfo
