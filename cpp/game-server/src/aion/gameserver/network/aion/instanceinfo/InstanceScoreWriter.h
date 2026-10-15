#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/network/PacketWriteHelper.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * Java generic InstanceScoreWriter<T>: erased to a non-template class, type variables spelled as their bounds (hub-headers.md §8.1).
 *
 * @author xTz
 */
class InstanceScoreWriter : public PacketWriteHelper {
	AION_MAKE_REF_FRIEND
protected:
	const runtime::Ref<model::instance::instancescore::InstanceScore> instanceScore;

	explicit InstanceScoreWriter(model::instance::instancescore::InstanceScore& instanceScore);

public:
	runtime::Ptr<model::instance::instancescore::InstanceScore> getInstanceScore() const { return this->instanceScore; }

	void writeMe(commons::utils::ByteBuffer& buf) override;

protected:
	~InstanceScoreWriter() override;
};

} // namespace aion::gameserver::network::aion::instanceinfo
