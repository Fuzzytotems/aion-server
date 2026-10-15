package com.aionemu.gameserver.controllers;

import java.util.List;

import com.aionemu.gameserver.model.gameobjects.Creature;

/**
 * Controls a creature.
 */
public abstract class CreatureController<T extends Creature> {

	private T owner;

	public CreatureController(T owner) {
		this.owner = owner;
	}

	public T getOwner() {
		return owner;
	}

	public abstract void onAttack(Creature attacker, int damage);

	public List<T> owners() {
		return List.of(owner);
	}
}
