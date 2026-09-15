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
 * C++: RefCounted like its base (create()). The score class it writes has no C++ declaration header yet (model.instance, P5-13), so the
 * constructors and bodies are AION_UNPORTED until it exists.
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
};

} // namespace aion::gameserver::network::aion::instanceinfo
