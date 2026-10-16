#pragma once

// Verbatim excerpts of the shipped static data (game-server/data/static_data) for the two hand-ported quests of chunk Q03 (P6-Q slice 2,
// 2026-09-29: heiron/_1643TheStarOfHeiron, heiron/_3200PriceOfGoodwill): each string below is one source line, copied unchanged; only the
// root elements are written by hand. Generated once from the Java data tree by a throw-away script of the lane (the ZoneQuestTestData.h
// convention); the file:line of every excerpt is in its comment.

#include <string>

#include "../instance/AscensionTestData.h"

namespace aion::gameserver::questEngine::handlers::q03::test {

using ::aion::gameserver::instance::test::joinLines;

/**
 * world_maps.xml:13, :14, :38, :139 (Verteron, where gs.scenario.travel's Elyos arrives: GeneratedHooksTest.cpp; Heiron; Steel Rake, the
 * instance 3200 creates; Reshanta, where its scroll teleports)
 */
inline const std::string& q03WorldMapsXml() {
	static const std::string xml = joinLines({
		R"x(<world_maps>)x",
		R"x(	<map id="210030000" cName="LF1A" name="Verteron" name_id="400261" twin_count="3" beginner_twin_count="4" max_user="200" water_level="100" death_level="0" world_type="ELYSEA" world_size="3072" drop_type="ELYSEA" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)x",
		R"x(	<map id="210040000" cName="LF3" name="Heiron" name_id="400263" beginner_twin_count="3" max_user="200" water_level="101" death_level="0" world_type="ELYSEA" world_size="3072" drop_type="ELYSEA" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)x",
		R"x(	<map id="300100000" cName="IDshulackShip" name="Steel Rake" name_id="401255" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="STEEL_RAKE_INSTANCE" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>)x",
		R"x(	<map id="400010000" cName="Ab1" name="Reshanta" name_id="400720" water_level="16" death_level="500" world_type="ABYSS" world_size="4096" drop_type="ABYSS" flags="BIND RECALL GLIDE FLY RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)x",
		R"x(</world_maps>)x",
	});
	return xml;
}

/** instance_cooltimes/instance_cooltimes.xml:173-182 (Steel Rake) */
inline const std::string& q03InstanceCooltimesXml() {
	static const std::string xml = joinLines({
		R"x(<instance_cooltimes>)x",
		R"x(	<instance_cooltime race="PC_ALL" worldId="300100000" id="18" sync_id="18">)x",
		R"x(		<type>DAILY</type>)x",
		R"x(		<ent_cool_time>900</ent_cool_time>)x",
		R"x(		<maxcount>5</maxcount>)x",
		R"x(		<max_member_light>6</max_member_light>)x",
		R"x(		<max_member_dark>6</max_member_dark>)x",
		R"x(		<enter_min_level_light>40</enter_min_level_light>)x",
		R"x(		<enter_min_level_dark>40</enter_min_level_dark>)x",
		R"x(		<can_enter_mentor>true</can_enter_mentor>)x",
		R"x(	</instance_cooltime>)x",
		R"x(</instance_cooltimes>)x",
	});
	return xml;
}

/** spawns/Instances/300100000_Steel Rake.xml:3 (the map element; its closing tag written by hand), :124-126, :128-130 (Haorunerk's corpse 798333, which 3200 replaces by Haorunerk 798332, and his bag 700522) */
inline const std::string& q03SpawnsXml() {
	static const std::string xml = joinLines({
		R"x(<spawns>)x",
		R"x(	<spawn_map map_id="300100000">)x",
		R"x(		<spawn npc_id="798333">)x",
		R"x(			<spot x="406.312" y="499.564" z="885.76"/>)x",
		R"x(		</spawn>)x",
		R"x(		<spawn npc_id="700522">)x",
		R"x(			<spot x="401.239" y="503.193" z="885.76" h="119"/>)x",
		R"x(		</spawn>)x",
		R"x(	</spawn_map>)x",
		R"x(</spawns>)x",
	});
	return xml;
}

/** npcs/npc_templates.xml:20099-20112 without its <equipment> (:20103-20109), :21012-21018, :21151-21166 without its <equipment> (:21155-21163), :21516-21522, :341295-341301, :442118-442122, :463837-463843, :463971-463977, :463978-463984: the npcs of 1643 (204545, 204614, 204630) and 3200 (204658, 279006, 700522, 798322, 798332, 798333) */
inline const std::string& q03NpcTemplatesXml() {
	static const std::string xml = joinLines({
		R"x(<npc_templates>)x",
		R"x(	<npc_template npc_id="204545" level="50" name="liske" name_id="351947" height="2" title_id="350438" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="14535">)x",
		R"x(			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />)x",
		R"x(		</stats>)x",
		R"x(		<bound_radius front="0.25" side="0.35" upper="2" />)x",
		R"x(		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(	<npc_template npc_id="204614" level="41" name="vengeful spirit of prapero" name_id="352025" height="1.52" group_drop="SKELETON" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="9883">)x",
		R"x(			<speeds walk="0.522" group_walk="0.522" run="7" run_fight="6" group_run_fight="4.2" />)x",
		R"x(		</stats>)x",
		R"x(		<bound_radius front="0.578" side="0.32" upper="1.52" />)x",
		R"x(		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(	<npc_template npc_id="204630" level="50" name="erato" name_id="352035" height="2" title_id="350427" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="14535">)x",
		R"x(			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />)x",
		R"x(		</stats>)x",
		R"x(		<bound_radius front="0.25" side="0.35" upper="2" />)x",
		R"x(		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(	<npc_template npc_id="204658" level="45" name="roikinerk" name_id="352062" height="1.16875" title_id="370101" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="11878">)x",
		R"x(			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />)x",
		R"x(		</stats>)x",
		R"x(		<bound_radius front="0.595" side="0.3774" upper="1.16875" />)x",
		R"x(		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(	<npc_template npc_id="279006" level="40" name="garkbinerk" name_id="314001" height="1.16875" title_id="315067" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="9419">)x",
		R"x(			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />)x",
		R"x(		</stats>)x",
		R"x(		<bound_radius front="0.595" side="0.3774" upper="1.16875" />)x",
		R"x(		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(	<npc_template npc_id="700522" level="1" name="haorunerk's bag" name_id="371261" height="2" group_drop="TREASUREBOX3" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="quest_use_item" sangle="0" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="172" />)x",
		R"x(		<bound_radius front="0.25" side="0.35" upper="2" />)x",
		R"x(		<talk_info distance="5" delay="3" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(	<npc_template npc_id="798322" level="45" name="kuruminerk" name_id="351431" height="1.375" title_id="370119" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="11878">)x",
		R"x(			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />)x",
		R"x(		</stats>)x",
		R"x(		<bound_radius front="0.7" side="0.444" upper="1.375" />)x",
		R"x(		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(	<npc_template npc_id="798332" level="45" name="haorunerk" name_id="351601" height="0.9625" title_id="370106" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="11878">)x",
		R"x(			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />)x",
		R"x(		</stats>)x",
		R"x(		<bound_radius front="0.595" side="0.3774" upper="0.9625" />)x",
		R"x(		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(	<npc_template npc_id="798333" level="45" name="haorunerk's corpse" name_id="351602" height="1.16875" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">)x",
		R"x(		<stats maxHp="11878">)x",
		R"x(			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />)x",
		R"x(		</stats>)x",
		R"x(		<bound_radius front="0.595" side="0.3774" upper="1.16875" />)x",
		R"x(		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />)x",
		R"x(	</npc_template>)x",
		R"x(</npc_templates>)x",
	});
	return xml;
}

/** tribe/tribe_relations.xml:466-468 (FIELD_OBJECT_ALL), :477-477 (FIELD_OBJECT_LIGHT), :719-721 (GENERAL), :2302-2305 (PC), :2306-2310 (PC_DARK), :2596-2596 (USEALL) */
inline const std::string& q03TribeRelationsXml() {
	static const std::string xml = joinLines({
		R"x(<tribe_relations>)x",
		R"x(    <tribe name="FIELD_OBJECT_ALL" base="FIELD_OBJECT_LIGHT">)x",
		R"x(        <neutral>PC_DARK</neutral>)x",
		R"x(    </tribe>)x",
		R"x(    <tribe name="FIELD_OBJECT_LIGHT"/>)x",
		R"x(    <tribe name="GENERAL">)x",
		R"x(        <none>NEUTRAL_DGUARD YDUMMY_DGUARD YDUMMY2_DGUARD LDF4B_SPARRING_DGUARD LDF4B_SPARRING_DGUARD2 LDF5_DUMMY1_DGUARD LDF5_DUMMY2_DGUARD LDF5_SPARRING1_DGUARD LDF5_SPARRING2_DGUARD</none>)x",
		R"x(    </tribe>)x",
		R"x(    <tribe name="PC">)x",
		R"x(        <friend>LIGHT_SUR_MOB LIGHT_LICH</friend>)x",
		R"x(        <none>LASBERG NEUTRAL_DGUARD YDUMMY_DGUARD YDUMMY2_DGUARD LDF4B_SPARRING_DGUARD LDF4B_SPARRING_DGUARD2 XDRAKAN_UNATTACK LDF5_DUMMY1_DGUARD LDF5_DUMMY2_DGUARD LDF5_SPARRING1_DGUARD LDF5_SPARRING2_DGUARD</none>)x",
		R"x(    </tribe>)x",
		R"x(    <tribe name="PC_DARK">)x",
		R"x(        <friend>DARK_SUR_MOB DARK_LICH</friend>)x",
		R"x(        <neutral>FIELD_OBJECT_ALL FIELD_OBJECT_ALL_HOSTILEMONSTER</neutral>)x",
		R"x(        <none>NEUTRAL_LGUARD YDUMMY_LGUARD YDUMMY2_LGUARD LDF4B_SPARRING_GUARD LDF4B_SPARRING_GUARD2 XDRAKAN_UNATTACK LDF5_DUMMY1_LGUARD LDF5_DUMMY2_LGUARD LDF5_SPARRING1_LGUARD LDF5_SPARRING2_LGUARD</none>)x",
		R"x(    </tribe>)x",
		R"x(    <tribe name="USEALL"/>)x",
		R"x(</tribe_relations>)x",
	});
	return xml;
}

/** quest_data/quest_data.xml:5702-5711, :5712-5722, :20165-20174 (1642, the finished quest 1643 starts after; 1643 3200) */
inline const std::string& q03QuestsXml() {
	static const std::string xml = joinLines({
		R"x(<quests>)x",
		R"x(	<quest id="1642" name="Know Your Anubites" nameId="1102841" quest_zone="Heiron" minlevel_permitted="42" max_repeat_count="1" race_permitted="ELYOS" category="QUEST" restricted="true">)x",
		R"x(		<collect_items>)x",
		R"x(			<collect_item item_id="182201763" count="10"/>)x",
		R"x(		</collect_items>)x",
		R"x(		<rewards gold="65310" exp="2574895">)x",
		R"x(			<reward_item item_id="186000005" count="4"/>)x",
		R"x(		</rewards>)x",
		R"x(		<quest_drop npc_id="212000" item_id="182201763" chance="80"/>)x",
		R"x(		<quest_drop npc_id="212001" item_id="182201763" chance="80"/>)x",
		R"x(	</quest>)x",
		R"x(	<quest id="1643" name="The Star of Heiron" nameId="1102842" quest_zone="Heiron" minlevel_permitted="42" max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST" restricted="true">)x",
		R"x(		<rewards gold="38350" exp="2574895">)x",
		R"x(			<reward_item item_id="186000005" count="4"/>)x",
		R"x(		</rewards>)x",
		R"x(		<start_conditions>)x",
		R"x(			<finished quest_id="1642"/>)x",
		R"x(		</start_conditions>)x",
		R"x(		<quest_work_items>)x",
		R"x(			<quest_work_item item_id="182201764"/>)x",
		R"x(		</quest_work_items>)x",
		R"x(	</quest>)x",
		R"x(	<quest id="3200" name="Price of Goodwill" nameId="1103023" quest_zone="Heiron" minlevel_permitted="40" max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST">)x",
		R"x(		<collect_items>)x",
		R"x(			<collect_item item_id="182209082" count="1"/>)x",
		R"x(		</collect_items>)x",
		R"x(		<rewards exp="1974220"/>)x",
		R"x(		<quest_drop npc_id="700522" item_id="182209082" drop_each_member="1" collecting_step="2"/>)x",
		R"x(		<quest_work_items>)x",
		R"x(			<quest_work_item item_id="182209082"/>)x",
		R"x(		</quest_work_items>)x",
		R"x(	</quest>)x",
		R"x(</quests>)x",
	});
	return xml;
}

/** items/item_templates.xml:894666-894666, :878138-878140, :884208-884211, :896245-896247 (182400001 kinah, 182201764 the Hazy Disk of 1643, 182209082 the Teleport Scroll of 3200, 186000005 1643's reward) */
inline const std::string& q03ItemTemplatesXml() {
	static const std::string xml = joinLines({
		R"x(<item_templates>)x",
		R"x(	<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>)x",
		R"x(	<item_template id="182201764" name="Hazy Disk" level="1" cName="quest_1643a" mask="20545" item_group="QUEST" quality="COMMON" price="1" desc="1107127">)x",
		R"x(		<inventory id="2"/>)x",
		R"x(	</item_template>)x",
		R"x(	<item_template id="182209082" name="Teleport Scroll" level="1" cName="QUEST_3200A" mask="20544" item_group="QUEST" quality="COMMON" price="1" desc="1110500" activate_target="STANDALONE" activate_count="1">)x",
		R"x(		<uselimits usedelay="2000" usedelayid="41" usearea="IDSHULACK_ITEMUSEAREA_Q3200"/>)x",
		R"x(		<inventory id="2"/>)x",
		R"x(	</item_template>)x",
		R"x(	<item_template id="186000005" name="Platinum Coin" level="50" cName="coin_05" mask="12410" max_stack_count="10000" item_group="COINS" quality="COMMON" price="18968" desc="703682">)x",
		R"x(		<inventory id="1"/>)x",
		R"x(	</item_template>)x",
		R"x(</item_templates>)x",
	});
	return xml;
}

/** player_experience_table.xml:3-68 (every level) */
inline const std::string& q03ExperienceTableXml() {
	static const std::string xml = joinLines({
		R"x(<player_experience_table>)x",
		R"x(	<exp>0</exp><!-- Level 0 --><!-- Experience is tallied up per level and stacks. -->)x",
		R"x(	<exp>400</exp><!-- 400 --><!-- Level 1 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1433</exp><!-- 1033 --><!-- Level 2 --><!-- NA 4.0 -->)x",
		R"x(	<exp>3820</exp><!-- 2387 --><!-- Level 3 --><!-- NA 4.0 -->)x",
		R"x(	<exp>9054</exp><!-- 5234 --><!-- Level 4 --><!-- NA 4.0 -->)x",
		R"x(	<exp>17655</exp><!-- 8601 --><!-- Level 5 --><!-- NA 4.0 -->)x",
		R"x(	<exp>30978</exp><!-- 13323 --><!-- Level 6 --><!-- NA 4.0 -->)x",
		R"x(	<exp>52010</exp><!-- 21032 --><!-- Level 7 --><!-- NA 4.0 -->)x",
		R"x(	<exp>82982</exp><!-- 30972 --><!-- Level 8 --><!-- NA 4.0 -->)x",
		R"x(	<exp>126069</exp><!-- 43087 --><!-- Level 9 --><!-- NA 4.0 -->)x",
		R"x(	<exp>182252</exp><!-- 56183 --><!-- Level 10 --><!-- NA 4.0 -->)x",
		R"x(	<exp>260622</exp><!-- 78370 --><!-- Level 11 --><!-- NA 4.0 -->)x",
		R"x(	<exp>360825</exp><!-- 100203 --><!-- Level 12 --><!-- NA 4.0 -->)x",
		R"x(	<exp>490331</exp><!-- 129506 --><!-- Level 13 --><!-- NA 4.0 -->)x",
		R"x(	<exp>649169</exp><!-- 158838 --><!-- Level 14 --><!-- NA 4.0 -->)x",
		R"x(	<exp>844378</exp><!-- 195209 --><!-- Level 15 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1083018</exp><!-- 238640 --><!-- Level 16 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1401356</exp><!-- 318338 --><!-- Level 17 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1808613</exp><!-- 407257 --><!-- Level 18 --><!-- NA 4.0 -->)x",
		R"x(	<exp>2314771</exp><!-- 506158 --><!-- Level 19 --><!-- NA 4.0 -->)x",
		R"x(	<exp>2941893</exp><!-- 627122 --><!-- Level 20 --><!-- NA 4.0 -->)x",
		R"x(	<exp>3769257</exp><!-- 827364 --><!-- Level 21 --><!-- NA 4.0 -->)x",
		R"x(	<exp>4811154</exp><!-- 1041897 --><!-- Level 22 --><!-- NA 4.0 -->)x",
		R"x(	<exp>6110198</exp><!-- 1299044 --><!-- Level 23 --><!-- NA 4.0 -->)x",
		R"x(	<exp>7632340</exp><!-- 1522142 --><!-- Level 24 --><!-- NA 4.0 -->)x",
		R"x(	<exp>9377726</exp><!-- 1745386 --><!-- Level 25 --><!-- NA 4.0 -->)x",
		R"x(	<exp>11395643</exp><!-- 2017917 --><!-- Level 26 --><!-- NA 4.0 -->)x",
		R"x(	<exp>13731725</exp><!-- 2336082 --><!-- Level 27 --><!-- NA 4.0 -->)x",
		R"x(	<exp>16339413</exp><!-- 2607688 --><!-- Level 28 --><!-- NA 4.0 -->)x",
		R"x(	<exp>19378549</exp><!-- 3039136 --><!-- Level 29 --><!-- NA 4.0 -->)x",
		R"x(	<exp>23162749</exp><!-- 3784200 --><!-- Level 30 --><!-- NA 4.0 -->)x",
		R"x(	<exp>27585843</exp><!-- 4423094 --><!-- Level 31 --><!-- NA 4.0 -->)x",
		R"x(	<exp>32841197</exp><!-- 5255354 --><!-- Level 32 --><!-- NA 4.0 -->)x",
		R"x(	<exp>39127217</exp><!-- 6286020 --><!-- Level 33 --><!-- NA 4.0 -->)x",
		R"x(	<exp>47350762</exp><!-- 8223545 --><!-- Level 34 --><!-- NA 4.0 -->)x",
		R"x(	<exp>57829684</exp><!-- 10478922 --><!-- Level 35 --><!-- NA 4.0 -->)x",
		R"x(	<exp>70654362</exp><!-- 12824678 --><!-- Level 36 --><!-- NA 4.0 -->)x",
		R"x(	<exp>87571065</exp><!-- 16916703 --><!-- Level 37 --><!-- NA 4.0 -->)x",
		R"x(	<exp>107018757</exp><!-- 19447692 --><!-- Level 38 --><!-- NA 4.0 -->)x",
		R"x(	<exp>129815732</exp><!-- 22796975 --><!-- Level 39 --><!-- NA 4.0 -->)x",
		R"x(	<exp>157211282</exp><!-- 27395550 --><!-- Level 40 --><!-- NA 4.0 -->)x",
		R"x(	<exp>189272188</exp><!-- 32060906 --><!-- Level 41 --><!-- NA 4.0 -->)x",
		R"x(	<exp>226933751</exp><!-- 37661563 --><!-- Level 42 --><!-- NA 4.0 -->)x",
		R"x(	<exp>267247400</exp><!-- 40313649 --><!-- Level 43 --><!-- NA 4.0 -->)x",
		R"x(	<exp>310053925</exp><!-- 42806525 --><!-- Level 44 --><!-- NA 4.0 -->)x",
		R"x(	<exp>355815203</exp><!-- 45761278 --><!-- Level 45 --><!-- NA 4.0 -->)x",
		R"x(	<exp>404823687</exp><!-- 49008484 --><!-- Level 46 --><!-- NA 4.0 -->)x",
		R"x(	<exp>456685353</exp><!-- 51861666 --><!-- Level 47 --><!-- NA 4.0 -->)x",
		R"x(	<exp>511683757</exp><!-- 54998404 --><!-- Level 48 --><!-- NA 4.0 -->)x",
		R"x(	<exp>570162075</exp><!-- 58478318 --><!-- Level 49 --><!-- NA 4.0 -->)x",
		R"x(	<exp>632268545</exp><!-- 62106470 --><!-- Level 50 --><!-- NA 4.0 -->)x",
		R"x(	<exp>701585822</exp><!-- 69317277 --><!-- Level 51 --><!-- NA 4.0 -->)x",
		R"x(	<exp>776831823</exp><!-- 75246001 --><!-- Level 52 --><!-- NA 4.0 -->)x",
		R"x(	<exp>857090855</exp><!-- 80259032 --><!-- Level 53 --><!-- NA 4.0 -->)x",
		R"x(	<exp>947120930</exp><!-- 90030075 --><!-- Level 54 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1051346275</exp><!-- 104225345 --><!-- Level 55 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1175571620</exp><!-- 124225345 --><!-- Level 56 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1318550121</exp><!-- 142978501 --><!-- Level 57 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1484090156</exp><!-- 165540035 --><!-- Level 58 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1674064804</exp><!-- 189974648 --><!-- Level 59 --><!-- NA 4.0 -->)x",
		R"x(	<exp>1913274732</exp><!-- 239209928 --><!-- Level 60 --><!-- NA 4.0 -->)x",
		R"x(	<exp>2162140395</exp><!-- 248865663 --><!-- Level 61 --><!-- NA 4.0 -->)x",
		R"x(	<exp>2419819338</exp><!-- 257678943 --><!-- Level 62 --><!-- NA 4.0 -->)x",
		R"x(	<exp>2700930959</exp><!-- 281111621 --><!-- Level 63 --><!-- NA 4.0 -->)x",
		R"x(	<exp>3209499233</exp><!-- 508568274 --><!-- Level 64 --><!-- NA 4.0 -->)x",
		R"x(	<exp>3794060468</exp><!-- 584561235 --><!-- Level 65 --><!-- NA 4.0 -->)x",
		R"x(</player_experience_table>)x",
	});
	return xml;
}

} // namespace aion::gameserver::questEngine::handlers::q03::test
