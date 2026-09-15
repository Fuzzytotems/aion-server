#include "aion/gameserver/network/aion/instanceinfo/LegionDominionScoreWriter.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
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
LegionDominionScoreWriter::LegionDominionScoreWriter(model::instance::instancescore::LegionDominionScore& reward)
    : InstanceScoreWriter(unportedScoreBase()) {}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

LegionDominionScoreWriter::~LegionDominionScoreWriter() = default;

runtime::Ref<LegionDominionScoreWriter> LegionDominionScoreWriter::create(model::instance::instancescore::LegionDominionScore& reward) {
	return runtime::makeRef<LegionDominionScoreWriter>(reward);
}

void LegionDominionScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::instanceinfo
