#include "aion/gameserver/network/aion/iteminfo/StigmaInfoBlobEntry.h"

#include <vector>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/Stigma.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::network::aion::iteminfo {

StigmaInfoBlobEntry::StigmaInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::STIGMA_INFO) {
}

StigmaInfoBlobEntry::~StigmaInfoBlobEntry() = default;

runtime::Ref<StigmaInfoBlobEntry> StigmaInfoBlobEntry::create() {
	return runtime::makeRef<StigmaInfoBlobEntry>();
}

void StigmaInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	const model::templates::item::Stigma* stigma = ownerItem->getItemTemplate()->getStigma();
	const std::vector<const skillengine::model::SkillTemplate*>* firstGroupSkills = stigma->getGainSkillsByGroup(1);
	const std::vector<const skillengine::model::SkillTemplate*>* secondGroupSkills = stigma->getGainSkillsByGroup(2);
	writeD(buf, firstGroupSkills == nullptr ? 0 : firstGroupSkills->at(0)->getSkillId()); // group 1 skill id
	writeD(buf, secondGroupSkills == nullptr ? 0 : secondGroupSkills->at(0)->getSkillId()); // group 2 skill id
	writeD(buf, 0); // Shard count in 4.7
	skip(buf, 192);
	writeH(buf, 0x1); // unk
	writeH(buf, 0);
	skip(buf, 96);
	writeH(buf, 0); // unk
}

int32_t StigmaInfoBlobEntry::getSize() {
	return 306; // 12 + 192 + 4 + 96 + 2
}

} // namespace aion::gameserver::network::aion::iteminfo
