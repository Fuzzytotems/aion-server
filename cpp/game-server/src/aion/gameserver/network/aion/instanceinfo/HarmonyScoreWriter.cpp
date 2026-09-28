#include "aion/gameserver/network/aion/instanceinfo/HarmonyScoreWriter.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
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
HarmonyScoreWriter::HarmonyScoreWriter(model::instance::instancescore::HarmonyArenaScore& reward, model::instance::InstanceScoreType typeValue,
                                       model::gameobjects::player::Player& owner)
    : InstanceScoreWriter(unportedScoreBase()), type(typeValue), playerObjId(owner.getObjectId()) {}

HarmonyScoreWriter::HarmonyScoreWriter(model::instance::instancescore::HarmonyArenaScore& reward, model::instance::InstanceScoreType typeValue)
    : InstanceScoreWriter(unportedScoreBase()), type(typeValue), playerObjId(0) {}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

HarmonyScoreWriter::~HarmonyScoreWriter() = default;

runtime::Ref<HarmonyScoreWriter> HarmonyScoreWriter::create(model::instance::instancescore::HarmonyArenaScore& reward,
                                                            model::instance::InstanceScoreType typeValue, model::gameobjects::player::Player& owner) {
	return runtime::makeRef<HarmonyScoreWriter>(reward, typeValue, owner);
}

runtime::Ref<HarmonyScoreWriter> HarmonyScoreWriter::create(model::instance::instancescore::HarmonyArenaScore& reward,
                                                            model::instance::InstanceScoreType typeValue) {
	return runtime::makeRef<HarmonyScoreWriter>(reward, typeValue);
}

void HarmonyScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

void HarmonyScoreWriter::writePoints(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem) {
	AION_UNPORTED();
}

void HarmonyScoreWriter::writeReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem) {
	AION_UNPORTED();
}

void HarmonyScoreWriter::writeSimpleReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::RewardItem* rewardItem) {
	network::detail::writeSimpleReward(buf, rewardItem);
}

} // namespace aion::gameserver::network::aion::instanceinfo
