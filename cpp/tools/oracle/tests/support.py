"""Fixture helpers: small static_data trees written to a temporary directory."""

from __future__ import annotations

import tempfile
from pathlib import Path

from staticdata_oracle.counts import ItemGroupsData

HEADER = '<?xml version="1.0" encoding="UTF-8"?>\n'

# Smallest content per StaticData element that satisfies every holder rule (so StaticData.afterUnmarshal would not throw).
MINIMAL_HOLDERS = {
	"world_maps": '<map id="1"/>',
	"weather": '<map id="1"/>',
	"npc_trade_list": '<tradelist_template npc_id="1"/><trade_in_list_template npc_id="1"/><purchase_template npc_id="1"/>',
	"npc_teleporter": '<teleporter_template teleportId="1"/>',
	"teleport_location": '<teleloc_template loc_id="1"/>',
	"bind_points": '<bind_point npcid="1"/>',
	"quests": '<quest id="1"/>',
	"quest_scripts": '<report_to id="7"/><xml_quest id="7"/><monster_hunt id="8"/>',
	"player_experience_table": '<exp>1</exp>',
	"absolute_stats": '<stats_set id="1"/>',
	"item_templates": '<item_template id="1"/>',
	"random_bonuses": '<random_bonus id="1" type="INVENTORY"/>',
	"npc_templates": '<npc_template npc_id="1" tribe="GENERAL"><stats/></npc_template>',
	"custom_drop": '<npc_drop npc_id="1"/>',
	"npc_shouts": '<shout_group><shout_npcs npc_ids="1"><shout/></shout_npcs></shout_group>',
	"player_initial_data": '<player_data class="WARRIOR"/>',
	"skill_data": '<skill_template skill_id="1"/>',
	"motion_times": '<motion_time name="a"/>',
	"skill_tree": '<skill skillId="1" minLevel="1" classId="WARRIOR"/>',
	"cube_expander": '<expansion_npc ids="1"/>',
	"warehouse_expander": '<expansion_npc ids="1"/>',
	"player_titles": '<title id="1"/>',
	"gatherable_templates": '<gatherable_template id="1"/>',
	"npc_walker": '<walker_template route_id="a"/>',
	"zones": '<zone name="z" mapid="1"><points/></zone>',
	"goodslists": '<list id="1"/><in_list id="1"/><purchase_list id="1"/>',
	"tribe_relations": '<tribe name="GENERAL"/>',
	"recipe_templates": '<recipe_template id="1"/>',
	"chest_templates": '<chest npc_id="1"/>',
	"staticdoor_templates": '<world world="1"/>',
	"item_sets": '<itemset id="1"><itempart/></itemset>',
	"npc_factions": '<npc_faction id="1"/>',
	"npc_skill_templates": '<npc_skills npc_ids="1"/>',
	"pet_skill_templates": '<pet_skill order_skill="1"/>',
	"siege_locations": '<siege_location id="1" type="FORTRESS"/>',
	"dimensional_vortex": '<vortex_location id="1"/>',
	"rift_locations": '<rift_location id="1"/>',
	"base_locations": '<base_location/>',
	"fly_rings": '',
	"shields": '',
	"pets": '<pet id="1"/>',
	"pet_feed": '',
	"dopings": '<doping id="1"/>',
	"pet_buffs": '',
	"guides": '<guide level="1"/>',
	"roads": '',
	"instance_cooltimes": '<instance_cooltime worldId="1"/>',
	"decomposable_items": '<decomposable item_id="1"><items/></decomposable>',
	"ai_templates": '<ai npcId="1"/>',
	"flypath_template": '<flypath_location id="1"/>',
	"windstreams": '<windstream mapid="1"/>',
	"item_restriction_cleanups": '',
	"assembled_npcs": '<assembled_npc nr="1"/>',
	"cosmetic_items": '<cosmetic_item cosmetic_name="a"/>',
	"auto_groups": '<auto_group id="1"/>',
	"timed_events": '',
	"spawns": '<spawn_map map_id="1"/>',
	"item_groups": "".join(f"<{g}/>" for g in ItemGroupsData.BONUS_GROUPS)
	+ "".join(f"<{g}/>" for g in ItemGroupsData.FOOD_GROUPS.values() if g),
	"polymorph_panels": '<panel panel_id="1"/>',
	"instance_bonusattrs": '<instance_bonusattr buff_id="1"/>',
	"housing_objects": '<chair id="1"/>',
	"rides": '<ride_info id="1"/>',
	"instance_exits": '<instance_exit/>',
	"portal_locs": '<portal_loc loc_id="1"/>',
	"portal_templates2": '',
	"house_lands": '<land><addresses><address id="1"/></addresses></land>',
	"buildings": '',
	"house_parts": '',
	"curing_objects": '<curing_object/>',
	"house_npcs": '<house address="1"><spawn type="MANAGER"/></house>',
	"assembly_items": '<item/>',
	"mails": '<mail name="A"/>',
	"material_templates": '',
	"challenge_tasks": '<task id="1"/>',
	"conqueror_protector_ranks": '<rank/>',
	"town_spawns_data": '<spawn_map map_id="1"><town_spawn town_id="1"><town_level level="1"><spawn/></town_level></town_spawn></spawn_map>',
	"skill_charge": '<charge id="1"><skill/></charge>',
	"walker_versions": '<walk_parent id="p"><version id="a"/></walk_parent>',
	"tempering_templates": '<tempering_list item_group="A"><tempering_data/></tempering_list>',
	"enchant_templates": '<enchant_list item_group="A"><enchant_data/></enchant_list>',
	"global_rules": '<gd_rule/>',
	"global_npc_exclusions": '',
	"multi_return_item": '<return_item id="1"/>',
	"hotspot_template": '',
	"item_purifications": '<item_purification base_item_id="1"><purification_result/></item_purification>',
	"arcadelist": '<rewards/>',
	"login_events": '<login_event id="1"/>',
	"world_raid_locations": '<world_raid_location location_id="1"/>',
	"kill_bounties": '<kill_bounty/>',
	"legion_dominion_template": '<legion_dominion_location/>',
	"alias_locations": '<alias_location name="a"/>',
	"signet_data_templates": '<signet_data_template signet_skill="SIGNET1"/>',
}


def xml(root: str, body: str = "", attrs: str = "") -> str:
	a = f" {attrs}" if attrs else ""
	return f"{HEADER}<{root}{a}>{body}</{root}>\n"


class Tree:
	"""A temporary static_data directory: tree.write('a/b.xml', text); tree.static_data(imports) writes static_data.xml."""

	def __init__(self):
		self._tmp = tempfile.TemporaryDirectory()
		self.root = Path(self._tmp.name) / "static_data"
		self.root.mkdir()

	def close(self):
		self._tmp.cleanup()

	def __enter__(self):
		return self

	def __exit__(self, *exc):
		self.close()

	def write(self, rel: str, text: str, raw: bytes | None = None) -> Path:
		path = self.root / rel
		path.parent.mkdir(parents=True, exist_ok=True)
		if raw is not None:
			path.write_bytes(raw)
		else:
			path.write_text(text, encoding="utf-8", newline="\n")
		return path

	def mkdir(self, rel: str) -> Path:
		path = self.root / rel
		path.mkdir(parents=True, exist_ok=True)
		return path

	def static_data(self, *imports: str) -> Path:
		body = "".join(f"\n\t<import {i} />" for i in imports)
		return self.write("static_data.xml", xml("static_data", body + "\n", 'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"'))

	def minimal(self, overrides: dict | None = None, skip=()) -> None:
		"""Writes one file per StaticData holder (content from MINIMAL_HOLDERS, replaced by overrides) and imports them all."""
		holders = dict(MINIMAL_HOLDERS)
		holders.update(overrides or {})
		imports = []
		for tag, body in holders.items():
			if tag in skip:
				continue
			self.write(f"{tag}.xml", xml(tag, body))
			imports.append(f'file="{tag}.xml"')
		self.static_data(*imports)
