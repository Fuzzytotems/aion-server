#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * @author Yeats
 * <p>
 * C++: RefCounted like its base (create()). The erased base holds the score as InstanceScore; score() is the typed field of Java's generic
 * InstanceScoreWriter (the constructor takes the score class).
 */
class LegionDominionScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND

protected:
	LegionDominionScoreWriter(model::instance::instancescore::LegionDominionScore& reward);
	~LegionDominionScoreWriter() override;

public:
	/** Java: new LegionDominionScoreWriter(...) */
	static runtime::Ref<LegionDominionScoreWriter> create(model::instance::instancescore::LegionDominionScore& reward);

	void writeMe(commons::utils::ByteBuffer& buf) override;

private:
	/** Java: the typed field instanceScore (InstanceScoreWriter<LegionDominionScore>) */
	model::instance::instancescore::LegionDominionScore& score() const;
};

} // namespace aion::gameserver::network::aion::instanceinfo
