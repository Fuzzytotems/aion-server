#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * @author xTz, Estrayl
 * <p>
 * C++: RefCounted like its base (create()). The erased base holds the score as InstanceScore; score() is the typed field of Java's generic
 * InstanceScoreWriter (the constructor takes the score class).
 */
class EternalBastionScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND

protected:
	EternalBastionScoreWriter(model::instance::instancescore::NormalScore& reward);
	~EternalBastionScoreWriter() override;

public:
	/** Java: new EternalBastionScoreWriter(...) */
	static runtime::Ref<EternalBastionScoreWriter> create(model::instance::instancescore::NormalScore& reward);

	void writeMe(commons::utils::ByteBuffer& buf) override;

private:
	/** Java: the typed field instanceScore (InstanceScoreWriter<NormalScore>) */
	model::instance::instancescore::NormalScore& score() const;
};

} // namespace aion::gameserver::network::aion::instanceinfo
