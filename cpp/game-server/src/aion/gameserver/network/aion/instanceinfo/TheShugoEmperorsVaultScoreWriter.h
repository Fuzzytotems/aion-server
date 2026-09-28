#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * Created by Yeats on 01.05.2016.
 * <p>
 * C++: RefCounted like its base (create()). The score class it writes has no C++ declaration header yet (model.instance, P5-13), so the
 * constructors and bodies are AION_UNPORTED until it exists.
 */
class TheShugoEmperorsVaultScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND

protected:
	TheShugoEmperorsVaultScoreWriter(model::instance::instancescore::NormalScore& reward);
	~TheShugoEmperorsVaultScoreWriter() override;

public:
	/** Java: new TheShugoEmperorsVaultScoreWriter(...) */
	static runtime::Ref<TheShugoEmperorsVaultScoreWriter> create(model::instance::instancescore::NormalScore& reward);

	void writeMe(commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::aion::instanceinfo
