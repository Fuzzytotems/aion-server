package com.aionemu.gameserver.controllers;

import com.aionemu.gameserver.model.gameobjects.Creature;
import com.aionemu.gameserver.model.gameobjects.player.Player;

public class PlayerController extends CreatureController<Player> {

	public PlayerController(Player owner) {
		super(owner);
	}

	@Override
	public void onAttack(Creature attacker, int damage) {
	}
}
