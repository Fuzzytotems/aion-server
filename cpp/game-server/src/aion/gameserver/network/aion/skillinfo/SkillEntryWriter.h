#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/network/PacketWriteHelper.h"
#include "aion/gameserver/network/aion/skillinfo/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"

namespace aion::gameserver::network::aion::skillinfo {

/**
 * Writes one player skill entry (SM_SKILL_LIST, SM_GM_SHOW_PLAYER_SKILLS). C++: RefCounted like its PacketWriteHelper base; writeSkillEntry
 * creates it on the stack's behalf through create().
 */
class SkillEntryWriter : public PacketWriteHelper {
	AION_MAKE_REF_FRIEND
public:
	static const runtime::PinnedCallback<int32_t(model::skill::PlayerSkillEntry&)> DYNAMIC_BODY_PART_SIZE_CALCULATOR;

private:
	const runtime::Ref<model::skill::PlayerSkillEntry> skillEntry;

public:
	static void writeSkillEntry(model::skill::PlayerSkillEntry& skillEntry, commons::utils::ByteBuffer& buffer);

	/** Java: new SkillEntryWriter(skillEntry) */
	static runtime::Ref<SkillEntryWriter> create(model::skill::PlayerSkillEntry& skillEntry);

protected:
	explicit SkillEntryWriter(model::skill::PlayerSkillEntry& skillEntry);
	~SkillEntryWriter() override;

	void writeMe(commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::aion::skillinfo
