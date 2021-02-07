#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"

class ActorState {
public:
	Vector location;
	Vector velocity;
	Rotator rotation;
	Vector angVelocity;

	ActorState();
	ActorState(ActorWrapper a);
	ActorState(ActorState lh, ActorState rh, float percent);
	ActorState(std::ifstream& in);

	void write(std::ofstream& out) const;
	void apply(ActorWrapper a) const;
};

class CarState {
public:
	ActorState actorState;
	float boostAmount;
	bool hasDodge;
	float lastJumped; // cannot apply; used to reset dodge in record().

	CarState();
	CarState(CarWrapper c);
	CarState(CarState lh, CarState rh, float percent);
	CarState(std::ifstream& in);

	void write(std::ofstream& out) const;
	void apply(CarWrapper c) const;
};

class GameState {
public:
	ActorState ball;
	CarState car;

	GameState();
	GameState(ServerWrapper sw);
	GameState(const GameState& lh, const GameState& rh, float percent);
	GameState(std::ifstream& in);

	void write(std::ofstream& out) const;
	void apply(ServerWrapper sw) const;
};
