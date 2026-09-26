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
 * C++: RefCounted like its base (create()). Java InstanceScore<CruciblePlayerReward> is the erased InstanceScore; writeMe reads
 * CruciblePlayerReward, which has no C++ declaration header yet (model.instance, P5-13), so it is AION_UNPORTED until it exists.
 */
class CrucibleScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND

protected:
	CrucibleScoreWriter(model::instance::instancescore::InstanceScore& reward);
	~CrucibleScoreWriter() override;

public:
	/** Java: new CrucibleScoreWriter(...) */
	static runtime::Ref<CrucibleScoreWriter> create(model::instance::instancescore::InstanceScore& reward);

	void writeMe(commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::aion::instanceinfo
