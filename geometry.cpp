/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"
#include "utils/parser.h"

Vector deflect(Vector velocity, float dir, float speed);
Vector randVec();
Vector avgVec(Vector a, Vector b, float amt);

GameState CheckpointPlugin::applyVariance(GameState& s) {
	int maxVar = cvarManager->getCvar("cpt_variance_tot").getIntValue();
	if (maxVar == 0) {
		return s;
	}
	float carDir = random(.0f, cvarManager->getCvar("cpt_variance_car_dir").getFloatValue());
	float carSpd = cvarManager->getCvar("cpt_variance_car_spd").getFloatValue();
	carSpd = random(-carSpd, carSpd);
	float carRot = cvarManager->getCvar("cpt_variance_car_rot").getFloatValue();
	float ballDir = random(.0f, cvarManager->getCvar("cpt_variance_ball_dir").getFloatValue());
	float ballSpd = cvarManager->getCvar("cpt_variance_ball_spd").getFloatValue();
	ballSpd = random(-ballSpd, ballSpd);
	float ballRot = cvarManager->getCvar("cpt_variance_ball_rot").getFloatValue();
	float totVar = abs(carDir) + abs(carSpd) + abs(carRot) + abs(ballDir) + abs(ballSpd) + abs(ballRot);
	if (totVar < 0.1) {
		return s;
	}
	GameState o(s);
	if (totVar > maxVar) {
		float scale = maxVar / totVar;
		carDir *= scale;
		carSpd *= scale;
		carRot *= scale;
		ballDir *= scale;
		ballSpd *= scale;
		ballRot *= scale;
	}
	if (debug) {
		log("applying variance: ball(" +
			std::to_string(ballDir) + "," + std::to_string(ballSpd) + "," + std::to_string(ballRot) + "); car(" +
			std::to_string(carDir) + "," + std::to_string(carSpd) + "," + std::to_string(carRot) + "); tot: " +
			std::to_string(totVar));
	}
	o.car.actorState.velocity = deflect(o.car.actorState.velocity, carDir, 1 + (carSpd / 100.0f));
	o.car.actorState.angVelocity = avgVec(o.car.actorState.angVelocity, randVec(), carRot/10.0f);
	o.ball.velocity = deflect(o.ball.velocity, ballDir, 1 + (ballSpd / 100.0f));
	o.ball.angVelocity = avgVec(o.ball.angVelocity, randVec(), ballRot/10.0f);
	return o;
}

Vector randVec() {
	Vector v;
	v.X = random(-1000.0f, 1000.0f);
	v.Y = random(-1000.0f, 1000.0f);
	v.Z = random(-1000.0f, 1000.0f);
	auto m = 6.0f / v.magnitude();
	v.X *= m;
	v.Y *= m;
	v.Z *= m;
	return v;
}

Vector avgVec(Vector a, Vector b, float amount) {
	float amt1 = 1 - amount;
	return (a * amt1) + (b * amount);
}

Rot VectorToRot(Vector vVector) {
	Rot rRotation;
	rRotation.Yaw = atan2(vVector.Y, vVector.X) * CONST_RadToUnrRot;
	rRotation.Pitch = atan2(vVector.Z, sqrtf(vVector.X * vVector.X + vVector.Y * vVector.Y)) * CONST_RadToUnrRot;
	rRotation.Roll = 0;
	return rRotation;
}

Quat RotToQuat(Rot rot) {
	float rotatorToRadian = ((CONST_PI_F / 180.f) * .5f) / CONST_DegToUnrRot;
	float sinPitch = sinf(rot.Pitch * rotatorToRadian);
	float cosPitch = cosf(rot.Pitch * rotatorToRadian);
	float sinYaw = sinf(rot.Yaw * rotatorToRadian);
	float cosYaw = cosf(rot.Yaw * rotatorToRadian);
	float sinRoll = sinf(rot.Roll * rotatorToRadian);
	float cosRoll = cosf(rot.Roll * rotatorToRadian);
	Quat convertedQuat;
	convertedQuat.X = (cosRoll * sinPitch * sinYaw) - (sinRoll * cosPitch * cosYaw);
	convertedQuat.Y = (-cosRoll * sinPitch * cosYaw) - (sinRoll * cosPitch * sinYaw);
	convertedQuat.Z = (cosRoll * cosPitch * sinYaw) - (sinRoll * sinPitch * cosYaw);
	convertedQuat.W = (cosRoll * cosPitch * cosYaw) + (sinRoll * sinPitch * sinYaw);
	return convertedQuat;
}

Vector deflect(Vector velocity, float dir, float speed) {
	Quat velQ = RotToQuat(VectorToRot(velocity));
	Quat pitchQ = RotToQuat({ dir * CONST_DegToUnrRot, 0, 0 });  // Deflect dir degrees
	Quat rollQ = RotToQuat({ 0, 0, random(-32768.0f, 32764.0f) }); // Random direction
	return RotateVectorWithQuat(RotateVectorWithQuat(RotateVectorWithQuat({ velocity.magnitude() * speed, 0, 0 }, pitchQ), rollQ), velQ);
}