#pragma once

#include <cstdint>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/DredgionRoom.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * @author xTz
 * <p>
 * C++: RefCounted like its base (create()). The erased base holds the score as InstanceScore; score() is the typed field of Java's generic
 * InstanceScoreWriter<PvpInstanceScore<PvpInstancePlayerReward>> (the constructor takes the score class). The two lists are copies of the
 * caller's (fieldmap: ArrayList<Ref<...>>); Java stores the caller's lists, which the writer only reads.
 */
class DredgionScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<model::gameobjects::player::Player>> players{AION_LOCK_CLASS(DredgionScoreWriter::players)};
	runtime::ArrayList<runtime::Ref<model::instance::DredgionRoom>> dredgionRooms{AION_LOCK_CLASS(DredgionScoreWriter::dredgionRooms)};

protected:
	DredgionScoreWriter(model::instance::instancescore::PvpInstanceScore& reward, std::vector<runtime::Ref<model::gameobjects::player::Player>> players,
	                    std::vector<runtime::Ref<model::instance::DredgionRoom>> dredgionRooms);
	~DredgionScoreWriter() override;

public:
	/** Java: new DredgionScoreWriter(reward, players, dredgionRooms) */
	static runtime::Ref<DredgionScoreWriter> create(model::instance::instancescore::PvpInstanceScore& reward,
	                                                std::vector<runtime::Ref<model::gameobjects::player::Player>> players,
	                                                std::vector<runtime::Ref<model::instance::DredgionRoom>> dredgionRooms);

	void writeMe(commons::utils::ByteBuffer& buf) override;

private:
	void fillTableWithGroup(commons::utils::ByteBuffer& buf, model::Race race);

	/** Java: the typed field instanceScore */
	model::instance::instancescore::PvpInstanceScore& score() const;
};

} // namespace aion::gameserver::network::aion::instanceinfo
