#include "aion/gameserver/network/aion/instanceinfo/PvpInstanceScoreWriter.h"

#include <vector>

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
PvpInstanceScoreWriter::PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
                                               model::instance::InstanceScoreType instanceScoreTypeValue)
    : InstanceScoreWriter(unportedScoreBase()), instanceScoreType(instanceScoreTypeValue), race(), objectId(0), status(0) {}

PvpInstanceScoreWriter::PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
                                               model::instance::InstanceScoreType instanceScoreTypeValue, model::Race raceValue)
    : InstanceScoreWriter(unportedScoreBase()), instanceScoreType(instanceScoreTypeValue), race(raceValue), objectId(0), status(0) {}

PvpInstanceScoreWriter::PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
                                               model::instance::InstanceScoreType instanceScoreTypeValue, int32_t objectIdValue, int32_t statusValue)
    : InstanceScoreWriter(unportedScoreBase()), instanceScoreType(instanceScoreTypeValue), race(), objectId(objectIdValue), status(statusValue) {}

PvpInstanceScoreWriter::PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
                                               model::instance::InstanceScoreType instanceScoreTypeValue,
                                               std::vector<runtime::Ref<model::gameobjects::player::Player>> participantsValue)
    : InstanceScoreWriter(unportedScoreBase()), instanceScoreType(instanceScoreTypeValue), race(), objectId(0), status(0) {
	participants.addAll(participantsValue); // Java stores the caller's list; nothing modifies it after construction
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

PvpInstanceScoreWriter::~PvpInstanceScoreWriter() = default;

runtime::Ref<PvpInstanceScoreWriter> PvpInstanceScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                                    model::instance::InstanceScoreType instanceScoreTypeValue) {
	return runtime::makeRef<PvpInstanceScoreWriter>(reward, instanceScoreTypeValue);
}

runtime::Ref<PvpInstanceScoreWriter> PvpInstanceScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                                    model::instance::InstanceScoreType instanceScoreTypeValue,
                                                                    model::Race raceValue) {
	return runtime::makeRef<PvpInstanceScoreWriter>(reward, instanceScoreTypeValue, raceValue);
}

runtime::Ref<PvpInstanceScoreWriter> PvpInstanceScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                                    model::instance::InstanceScoreType instanceScoreTypeValue, int32_t objectIdValue,
                                                                    int32_t statusValue) {
	return runtime::makeRef<PvpInstanceScoreWriter>(reward, instanceScoreTypeValue, objectIdValue, statusValue);
}

runtime::Ref<PvpInstanceScoreWriter> PvpInstanceScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                                    model::instance::InstanceScoreType instanceScoreTypeValue,
                                                                    std::vector<runtime::Ref<model::gameobjects::player::Player>> participantsValue) {
	return runtime::makeRef<PvpInstanceScoreWriter>(reward, instanceScoreTypeValue, std::move(participantsValue));
}

void PvpInstanceScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

void PvpInstanceScoreWriter::writeShowReward(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

void PvpInstanceScoreWriter::writeUpdateScoreToBuffer(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

void PvpInstanceScoreWriter::writePlayerFullData(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

void PvpInstanceScoreWriter::writePlayerFullData(commons::utils::ByteBuffer& buf,
                                                 const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	AION_UNPORTED();
}

void PvpInstanceScoreWriter::writePlayerBuffInfo(commons::utils::ByteBuffer& buf,
                                                 const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	network::detail::writePlayerBuffInfo(buf, players, MAXIMUM_PLAYER_COUNT_PER_FACTION);
}

void PvpInstanceScoreWriter::writeEmptyDataToBuffer(commons::utils::ByteBuffer& buf, int32_t dataSize, int32_t missingPlayerCount) {
	network::detail::writeEmptyData(buf, dataSize, missingPlayerCount); // Java: new byte[negative] throws NegativeArraySizeException
}

} // namespace aion::gameserver::network::aion::instanceinfo
