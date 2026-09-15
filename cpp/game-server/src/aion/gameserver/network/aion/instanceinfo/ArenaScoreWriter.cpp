#include "aion/gameserver/network/aion/instanceinfo/ArenaScoreWriter.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/network/detail/InstanceInfoWriting.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::instanceinfo {

namespace {

/**
 * The InstanceScore base of the score passed to a constructor. The score class has no C++ declaration header yet (P5-13), so the conversion
 * cannot be written: reaching a constructor throws UnportedException before the base is initialized.
 */
[[noreturn]] model::instance::instancescore::InstanceScore& unportedScoreBase() {
	AION_UNPORTED();
}

} // namespace

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // the base initializer never returns until the score class is declared (P5-13)
#endif
ArenaScoreWriter::ArenaScoreWriter(model::instance::instancescore::PvPArenaScore& score, int32_t ownerObjectIdValue, bool rewardTableValue)
    : InstanceScoreWriter(unportedScoreBase()), ownerObjectId(ownerObjectIdValue), rewardTable(rewardTableValue) {}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

ArenaScoreWriter::~ArenaScoreWriter() = default;

runtime::Ref<ArenaScoreWriter> ArenaScoreWriter::create(model::instance::instancescore::PvPArenaScore& score, int32_t ownerObjectIdValue,
                                                        bool rewardTableValue) {
	return runtime::makeRef<ArenaScoreWriter>(score, ownerObjectIdValue, rewardTableValue);
}

void ArenaScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

void ArenaScoreWriter::writePlayerScores(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

void ArenaScoreWriter::writeOwnerRewards(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

void ArenaScoreWriter::writeReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem) {
	AION_UNPORTED();
}

void ArenaScoreWriter::writeSimpleReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::RewardItem* rewardItem) {
	network::detail::writeSimpleReward(buf, rewardItem);
}

} // namespace aion::gameserver::network::aion::instanceinfo
