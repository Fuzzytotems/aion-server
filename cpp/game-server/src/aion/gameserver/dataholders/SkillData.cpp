#include "aion/gameserver/dataholders/SkillData.h"

#include <cstddef>
#include <utility>

namespace aion::gameserver::dataholders {

namespace {

using skillengine::model::SkillTemplate;

/** Java HashMap.hash(Integer): the key's hashCode with its high bits spread into the low bits */
uint32_t spread(int32_t key) noexcept {
	const uint32_t h = static_cast<uint32_t>(key);
	return h ^ (h >> 16);
}

/**
 * Java: new HashMap<Integer, V>() filled with put(key, value) in the given order, then values(): the buckets in index order, each bucket in the
 * order of its linked list. Models putVal (a new key is appended to its bucket, a present key keeps its position and gets the new value),
 * resize (the table doubles when size exceeds 0.75 of the capacity; splitting a bucket keeps the relative order) and treeifyBin's resize of
 * tables below 64 buckets. Not modelled: a bucket that would become a red-black tree (8 keys in one bucket of a table of 64 or more buckets
 * reorders its list); the skill data never has more than 2 keys in a bucket.
 */
std::vector<const SkillTemplate*> javaHashMapValues(const std::vector<std::pair<int32_t, const SkillTemplate*>>& puts) {
	using Bucket = std::vector<std::pair<int32_t, const SkillTemplate*>>;
	std::vector<Bucket> table;
	size_t size = 0;
	auto resize = [&table] {
		const size_t newCapacity = table.empty() ? 16 : table.size() * 2;
		std::vector<Bucket> newTable(newCapacity);
		for (const Bucket& bucket : table) {
			for (const auto& entry : bucket)
				newTable[spread(entry.first) & (newCapacity - 1)].push_back(entry);
		}
		table = std::move(newTable);
	};
	for (const auto& [key, value] : puts) {
		if (table.empty())
			resize();
		Bucket& bucket = table[spread(key) & (table.size() - 1)];
		bool present = false;
		for (auto& entry : bucket) {
			if (entry.first == key) {
				entry.second = value;
				present = true;
				break;
			}
		}
		if (present)
			continue;
		const size_t previousLength = bucket.size();
		bucket.emplace_back(key, value);
		if (previousLength >= 8 && table.size() < 64) // TREEIFY_THRESHOLD, MIN_TREEIFY_CAPACITY: treeifyBin resizes instead
			resize();
		if (++size > table.size() / 4 * 3)
			resize();
	}
	std::vector<const SkillTemplate*> values;
	values.reserve(size);
	for (const Bucket& bucket : table) {
		for (const auto& entry : bucket)
			values.push_back(entry.second);
	}
	return values;
}

} // namespace

void SkillData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	skillTemplateById.clear();
	skillTemplatesByGroup.clear();
	skillTemplatesByStack.clear();
	std::vector<std::pair<int32_t, const SkillTemplate*>> puts;
	puts.reserve(skillTemplates.size());
	for (const SkillTemplate& skillTemplate : skillTemplates) {
		int32_t skillId = skillTemplate.getSkillId();
		skillTemplateById.insert_or_assign(skillId, &skillTemplate);
		puts.emplace_back(skillId, &skillTemplate);
		if (!skillTemplate.getGroup().empty()) // Java: getGroup() != null (absent attribute)
			skillTemplatesByGroup[skillTemplate.getGroup()].push_back(&skillTemplate);
		skillTemplatesByStack[skillTemplate.getStack()].push_back(&skillTemplate); // Java: getStack() != null, always (a required attribute)
	}
	skillTemplatesInHashOrder = javaHashMapValues(puts);
	// Java: skillTemplates = null (the C++ indexes point into the storage, which stays)
}

const SkillTemplate* SkillData::getSkillTemplate(int32_t skillId) const {
	auto it = skillTemplateById.find(skillId);
	return it != skillTemplateById.end() ? it->second : nullptr;
}

const std::vector<const SkillTemplate*>* SkillData::getSkillTemplatesByStack(std::string_view skillStack) const {
	auto it = skillTemplatesByStack.find(skillStack);
	return it != skillTemplatesByStack.end() ? &it->second : nullptr;
}

int32_t SkillData::size() const {
	return static_cast<int32_t>(skillTemplateById.size());
}

const std::vector<const SkillTemplate*>* SkillData::getSkillTemplatesByGroup(std::string_view skillGroup) const {
	auto it = skillTemplatesByGroup.find(skillGroup);
	return it != skillTemplatesByGroup.end() ? &it->second : nullptr;
}

std::vector<const SkillTemplate*> SkillData::getSkillTemplates() const {
	return skillTemplatesInHashOrder;
}

} // namespace aion::gameserver::dataholders
