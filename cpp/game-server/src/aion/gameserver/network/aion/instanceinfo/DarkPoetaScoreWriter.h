#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * @author xTz
 * <p>
 * C++: RefCounted like its base (create()). The erased base holds the score as InstanceScore; score() is the typed field of Java's generic
 * InstanceScoreWriter (the constructor takes the score class).
 */
class DarkPoetaScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND

protected:
	DarkPoetaScoreWriter(model::instance::instancescore::DarkPoetaScore& reward);
	~DarkPoetaScoreWriter() override;

public:
	/** Java: new DarkPoetaScoreWriter(...) */
	static runtime::Ref<DarkPoetaScoreWriter> create(model::instance::instancescore::DarkPoetaScore& reward);

	void writeMe(commons::utils::ByteBuffer& buf) override;

private:
	/** Java: the typed field instanceScore (InstanceScoreWriter<DarkPoetaScore>) */
	model::instance::instancescore::DarkPoetaScore& score() const;
};

} // namespace aion::gameserver::network::aion::instanceinfo
