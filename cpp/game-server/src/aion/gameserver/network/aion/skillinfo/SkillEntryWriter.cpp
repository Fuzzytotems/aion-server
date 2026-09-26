#include "aion/gameserver/network/aion/skillinfo/SkillEntryWriter.h"

#include "aion/gameserver/model/skill/PlayerSkillEntry.h"

namespace aion::gameserver::network::aion::skillinfo {

namespace {

/** Java: the lambda of DYNAMIC_BODY_PART_SIZE_CALCULATOR ((skill) -> 11) */
struct DynamicBodyPartSizeCalculator : runtime::TaskStruct {
	int32_t operator()(model::skill::PlayerSkillEntry&) const { return 11; }
};

} // namespace

const runtime::PinnedCallback<int32_t(model::skill::PlayerSkillEntry&)> SkillEntryWriter::DYNAMIC_BODY_PART_SIZE_CALCULATOR{DynamicBodyPartSizeCalculator{}};

void SkillEntryWriter::writeSkillEntry(model::skill::PlayerSkillEntry& skillEntry, commons::utils::ByteBuffer& buffer) {
	runtime::Ref<SkillEntryWriter> entryWriter = create(skillEntry);
	entryWriter->writeMe(buffer);
}

runtime::Ref<SkillEntryWriter> SkillEntryWriter::create(model::skill::PlayerSkillEntry& skillEntryValue) {
	return runtime::makeRef<SkillEntryWriter>(skillEntryValue);
}

SkillEntryWriter::SkillEntryWriter(model::skill::PlayerSkillEntry& skillEntryValue) : skillEntry(skillEntryValue) {
}

SkillEntryWriter::~SkillEntryWriter() = default;

void SkillEntryWriter::writeMe(commons::utils::ByteBuffer& buf) {
	writeH(buf, skillEntry->getSkillId());
	writeH(buf, skillEntry->isNormalSkill() ? 1 : skillEntry->getSkillLevel());
	writeC(buf, 0x00);
	writeC(buf, skillEntry->getProfessionSkillBarSize());
	writeD(buf, skillEntry->isProfessionSkill() ? skillEntry->getProfessionFlag() : skillEntry->getFlag());
	writeC(buf, skillEntry->getSkillType()); // 0 normal skill , 1 stigma skill , 3 linked stigma skill
}

} // namespace aion::gameserver::network::aion::skillinfo
