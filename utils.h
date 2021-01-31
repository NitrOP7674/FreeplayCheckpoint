/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fstream>
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "utils/customrotator.h"

static const float MAX_DODGE_TIME = 1.2f;

template<typename T>
void writePOD(std::ofstream& out, T& t) {
	out.write(reinterpret_cast<char*>(&t), sizeof(T));
}

template<typename T>
void readPOD(std::ifstream& in, T& t) {
	in.read(reinterpret_cast<char*>(&t), sizeof(T));
}
static inline void readVec(std::ifstream& in, Vector& v) {
	readPOD(in, v.X);
	readPOD(in, v.Y);
	readPOD(in, v.Z);
}
static inline void writeVec(std::ofstream& out, Vector& v) {
	writePOD(out, v.X);
	writePOD(out, v.Y);
	writePOD(out, v.Z);
}
static inline void readRot(std::ifstream& in, Rotator& r) {
	readPOD(in, r.Pitch);
	readPOD(in, r.Yaw);
	readPOD(in, r.Roll);
}
static inline void writeRot(std::ofstream& out, Rotator& r) {
	writePOD(out, r.Pitch);
	writePOD(out, r.Yaw);
	writePOD(out, r.Roll);
}

// Rotator uses ints instead of floats.  Floats are better.
struct Rot {
	float Pitch, Yaw, Roll;
};

class GameState {
public:
	Vector ballLocation;
	Vector carLocation;
	Vector ballVelocity;
	Vector carVelocity;
	Rotator ballRotation;
	Rotator carRotation;
	Vector ballAngVelocity;
	Vector carAngVelocity;
	float boostAmount;
	bool hasDodge;
	float lastJumped; // cannot apply; used to reset dodge in record().

	std::string string(Vector v) {
		return "(" + std::to_string(v.X) + "," + std::to_string(v.Y) + "," + std::to_string(v.Z) + ")";
	}
	std::string string(Rotator r) {
		return "(" + std::to_string(r.Pitch) + "," + std::to_string(r.Yaw) + "," + std::to_string(r.Roll) + ")";
	}
	std::string string() {
		return "carRotation: " + string(carRotation) + " asVec: " + string(RotatorToVector(carRotation));
		return "ball: " + string(ballLocation) + " / " + string(ballVelocity) + " / " + string(ballRotation) + " / " + string(ballAngVelocity) +
			"; car: " + string(carLocation) + " / " + string(carVelocity) + " / " + string(carRotation) + " / " + string(carAngVelocity) +
			"; boost: " + std::to_string(boostAmount) + "; dodge: " + std::to_string(hasDodge) + "; lastJumped: " + std::to_string(lastJumped);
	}

	GameState() {
		ballLocation = Vector(0, 0, 0);
		carLocation = Vector(0, 0, 0);
		ballVelocity = Vector(0, 0, 0);
		carVelocity = Vector(0, 0, 0);
		ballRotation = Rotator(0, 0, 0);
		carRotation = Rotator(0, 0, 0);
		ballAngVelocity = Vector(0, 0, 0);
		carAngVelocity = Vector(0, 0, 0);
		boostAmount = 0;
		hasDodge = false;
		lastJumped = 0;
	}
	GameState(std::ifstream& in) {
		readVec(in, ballLocation);
		readVec(in, carLocation);
		readVec(in, ballVelocity);
		readVec(in, carVelocity);
		readRot(in, ballRotation);
		readRot(in, carRotation);
		readVec(in, ballAngVelocity);
		readVec(in, carAngVelocity);
		readPOD(in, boostAmount);
		readPOD(in, hasDodge);
		readPOD(in, lastJumped);
	}

	void write(std::ofstream& out) {
		writeVec(out, ballLocation);
		writeVec(out, carLocation);
		writeVec(out, ballVelocity);
		writeVec(out, carVelocity);
		writeRot(out, ballRotation);
		writeRot(out, carRotation);
		writeVec(out, ballAngVelocity);
		writeVec(out, carAngVelocity);
		writePOD(out, boostAmount);
		writePOD(out, hasDodge);
		writePOD(out, lastJumped);
	}

	GameState(ServerWrapper sw) {
		BallWrapper b = sw.GetBall();
		CarWrapper c = sw.GetGameCar();
		ballLocation = b.GetLocation();
		carLocation = c.GetLocation();
		ballVelocity = b.GetVelocity();
		carVelocity = c.GetVelocity();
		ballRotation = b.GetRotation();
		carRotation = c.GetRotation();
		ballAngVelocity = b.GetAngularVelocity();
		carAngVelocity = c.GetAngularVelocity();
		boostAmount = c.GetBoostComponent().IsNull() ? 0 : c.GetBoostComponent().GetCurrentBoostAmount();

		// Save last jump time only if the player jumped.
		// After applying this, we will remove the player's dodge when the jump timer expires.
		lastJumped = !c.GetbJumped() || c.GetJumpComponent().IsNull() ? -1 : c.GetJumpComponent().GetInactiveTime();
		hasDodge = !c.GetbDoubleJumped() && lastJumped < MAX_DODGE_TIME;
	}

	// Returns the game state <percent (0-1.0)> way between lh and rh.
	GameState(GameState lh, GameState rh, float percent) {
		float rhPercent = 1 - percent;
		ballLocation = lh.ballLocation*percent + rh.ballLocation*rhPercent;
		carLocation = lh.carLocation*percent + rh.carLocation*rhPercent;
		ballVelocity = lh.ballVelocity*percent + rh.ballVelocity*rhPercent;
		carVelocity = lh.carVelocity*percent + rh.carVelocity*rhPercent;

		/* Custom Rotator */
		// TODO: There's a weird blip that goes slightly sideways when crossing verticle.
		// Figure out a better way to interpolate.
		CustomRotator rotator(percent);
		CustomRotator br(rh.ballRotation);
		CustomRotator bdiff = CustomRotator(lh.ballRotation).diffTo(br) * rotator;
		ballRotation = (br - bdiff).ToRotator();

		CustomRotator cr = CustomRotator(rh.carRotation);
		CustomRotator cdiff = CustomRotator(lh.carRotation).diffTo(cr) * rotator;
		carRotation = (cr - cdiff).ToRotator();

		ballAngVelocity = lh.ballAngVelocity*percent + rh.ballAngVelocity*rhPercent;
		carAngVelocity = lh.carAngVelocity*percent + rh.carAngVelocity*rhPercent;
		boostAmount = lh.boostAmount*percent + rh.boostAmount*rhPercent;

		if (lh.lastJumped == -1 || rh.lastJumped == -1) {
			lastJumped = -1;
			hasDodge = true;
		} else if (rh.lastJumped < lh.lastJumped) {
			lastJumped = 0;
			hasDodge = true;
		} else { // lh.lastJumped <= rh.lastJumped
			lastJumped = lh.lastJumped*percent + rh.lastJumped*rhPercent;
			hasDodge = lastJumped < MAX_DODGE_TIME;
		}
	}

	void apply(ServerWrapper tw) {
		BallWrapper b = tw.GetBall();
		CarWrapper c = tw.GetGameCar();
		b.SetLocation(ballLocation);
		c.SetLocation(carLocation);
		b.SetVelocity(ballVelocity);
		c.SetVelocity(carVelocity);
		b.SetRotation(ballRotation);
		c.SetRotation(carRotation);

		b.SetAngularVelocity(ballAngVelocity, 0);
		c.SetAngularVelocity(carAngVelocity, 0);
		if (!c.GetBoostComponent().IsNull()) {
			c.GetBoostComponent().SetCurrentBoostAmount(boostAmount);
		}
		c.SetbDoubleJumped(!hasDodge);
		c.SetbJumped(!hasDodge);
	}
};
