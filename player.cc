#include <math.h>

#include "player.h"
#include "level.h"
#include "game.h"

Player::Player() : Object() {
	jumpstart = 0;
	jumpplay = false;
	previousButtons = 0;
	dietime = 0;

	thrusting = false;
	thrusttime = 0;

	camagl = 3.141592 / 8;
	camzoom = 15;

	q3dQuaternionInit(&qRot);
}

Player::~Player() {
}

void Player::setController(int port) {
	this->port = port;
}

extern q3dTypePolyhedron *sphere;
extern q3dTypeFiller fillerPlayers;
extern uint32 gametime;

void Player::update(Game *game) {

	maple_device_t *dev = maple_enum_dev(port, 0);
	float ballradius = 3.0f;
	float level = 0;

	// find out where in the level we are
	float px = (LEVELSIZE + position.x) / (2 * LEVELSIZE);
	float pz = (LEVELSIZE + position.z) / (2 * LEVELSIZE);

	camadd = 0;
	int pos = (int) (pz * 32) *32 + (int) (px*32);
	if (px < 0 || pz < 0 || px > 1 || pz > 1) {
		// outside of level
		q3dColorSet3f(&color, 1.0f, 0.0f, 0.0f);
		level = -100;
	} else if (levelData[pos] == HOLE) {
		// we are above a hole
		q3dColorSet3f(&color, 1.0f, 1.0f, 0.0f);
		level = -100;
	} else if (levelData[pos] == HIGH) {
		// we are at a cube
		level = 10;
		q3dColorSet3f(&color, 0.0f, 0.0f, 1.0f);
//		camadd = 10;
	} else {
		// solid ground
		q3dColorSet3f(&color, 0.0f, 1.0f, 0.0f);
	}
	bool inair = position.y >= level + 0.1f + ballradius;

	if (!game->gameended && !dietime && dev != NULL && dev->info.functions & MAPLE_FUNC_CONTROLLER) {
		cont_state_t* s = (cont_state_t*) maple_dev_status(dev);
		float cx = s->joyx / 2048.0f;
		float speed = -s->joyy / 7000.0f;

		if (s->buttons & CONT_DPAD_UP) {
			camagl += 1.0f / 64.0f;
		} else if (s->buttons & CONT_DPAD_DOWN) {
			camagl -= 1.0f / 64.0f;
		}

		if (s->buttons & CONT_DPAD_LEFT) {
			camzoom -= 1.0f / 4.0f;
		} else if (s->buttons & CONT_DPAD_RIGHT) {
			camzoom += 1.0f / 4.0f;
		}

		if (camzoom < 5) camzoom = 5;
		else if (camzoom > 40) camzoom = 40;

		if (camagl < 0) camagl = 0;
		else if (camagl > 3.141592/2) camagl = 3.141592/2;

		if (s->buttons & CONT_A) {
			if (jumpplay && (gametime - jumpstart) < 400) {
				direction.y = 0.65f;
			}
			if (!jumpplay && (gametime - jumpstart) < 125) {
				snd_sfx_play(sounds[JUMP], 255, 0);
				jumpplay = true;
			}
		} else {
			if (!inair) {
				jumpstart = gametime;
				jumpplay = false;
			}
		}

		if (s->buttons & CONT_X) {
			if (thrusting && (gametime - thrusttime) < 50) {
				speed = 0.5;
			}
			if (!thrusting) {
//				snd_sfx_play(sounds[JUMP], 255, 0);
				thrusting = true;
				thrusttime = gametime;
			}
		} else {
			if (thrusting  && (gametime - thrusttime) > 250) {
				thrusting = false;
			}
		}

		rotation.y += cx;

		direction.x += sin(rotation.y) * speed;
		direction.z += cos(rotation.y) * speed;
//		direction.x = direction.x < -2 ? -2 : direction.x > 2 ? 2 : direction.x;
//		direction.z = direction.z < -2 ? -2 : direction.z > 2 ? 2 : direction.z;

		previousButtons = s->buttons;
	}

	// Update position and stuff
	direction.x *= 0.975f;
	direction.z *= 0.975f;

	position.x += direction.x;
	position.y += direction.y;
	position.z += direction.z;

	rotation.x -= direction.x * 0.25;
	rotation.z += direction.z * 0.25;

	float mul = 3.1415927 * 2 * 0.25 * 0.33;
	float a = -direction.z * mul * 0.5;
	static q3dTypeQuaternion qx;
	qx.w = cos(a);
	qx.x = sin(a);
	qx.y = qx.z = 0;

	a = direction.x * mul * 0.5;
	static q3dTypeQuaternion qz;
	qz.w = cos(a);
	qz.x = qz.y = 0;
	qz.z = sin(a);

	q3dQuaternionNormalize(&qx);
	q3dQuaternionNormalize(&qz);
/*
Qx = [ cos(a/2), (sin(a/2), 0, 0)]
Qy = [ cos(b/2), (0, sin(b/2), 0)]
Qz = [ cos(c/2), (0, 0, sin(c/2))]

And the final quaternion is obtained by Qx * Qy * Qz.
*/
	q3dQuaternionMul(&qx, &qz);
	q3dQuaternionMul(&qRot, &qx);
//	q3dColorSet3f(&color, 0.0f, 1.0f, 0.0f);
	// do collision detection between spheres.
	// this sphere has allready been tested against the previous
	// spheres so we don't need to test with those again.

	for (int i = port+1; i < 4; i++) {
		Player *p = &game->player[i];
		Point3D v(position);
		v -= p->position;

		float dist = v.x * v.x + v.y * v.y + v.z * v.z;
		float minDist = 2 * ballradius;
		minDist *= minDist;

		if (dist < minDist) {
			float len = fsqrt(dist);

			// objects are colliding
			q3dColorSet3f(&color, 1.0f, 0.0f, 0.0f);
			q3dColorSet3f(&game->player[i].color, 1.0f, 0.0f, 0.0f);

			Point3D temp(direction);
			Point3D v2(v);

			Point3D normal = v;
			normal.normalize();
			Point3D reflection = direction - 2*(direction.dot(normal)) * normal;

			normal.x = -normal.x;
			normal.y = -normal.y;
			normal.z = -normal.z;
			Point3D reflection2 = p->direction - 2*(p->direction.dot(normal)) * normal;

			direction = reflection *0.5 - reflection2 * 0.5;
			p->direction = reflection2 * 0.5 - reflection *0.5;

			len *= 2 * 2;
			v /= len;

			position += v;
			p->position -= v;

			snd_sfx_play(sounds[BOUNCE], 255, 0);
		} 
	}

	// Collide with boxes
	if (!dietime && position.y < 10 + ballradius) {
		px *= 32;
		pz *= 32;

		int ipx = (int) px;
		int ipz = (int) pz;

		// fractions
		float fpx = px - ipx;
		float fpz = pz - ipz;

		int c1 = fpx < 0.5 ? -1 : 1; // collide left/right
		int c2 = fpz < 0.5 ? -1 : 1; // collide top/bottom

		int pos1 = ipx + c1;
		int pos2 = ipz + c2;


		if (!(pos1 < 0 || pos1 >= 32)) {
			pos1 += ipz * 32;
			if (levelData[pos1] == HIGH && (fpx < 0.3 || fpx > 0.7)) {
				float add = fpx < 0.3 ? 0.31 : 0.69;
				position.x = (float) (ipx + add) / 32.0f * 2 * LEVELSIZE - LEVELSIZE;
				direction.x = -direction.x * 0.5;
			}
		}

		if (!(pos2 < 0 || pos2 >= 32)) {
			pos2 *= 32;
			pos2 += ipx;
			if (levelData[pos2] == HIGH && (fpz < 0.3 || fpz > 0.7)) {
				float add = fpz < 0.3 ? 0.31 : 0.69;
				position.z = ((float) (ipz + add) / 32.0f * 2 * LEVELSIZE - LEVELSIZE);
				direction.z = -direction.z * 0.5;
			}
		}
	}

	// TODO: proper collusion detection
	if (!dietime && position.y < level + ballradius) {
		position.y = level + ballradius;
		float val = 0.75;

		if (fabs(direction.y) < 1.0f) {
			val = direction.y * direction.y  * val;
		}
		direction.y = -direction.y * val;
		if (val < 0.1) direction.y = 0;
		snd_sfx_play(sounds[BOUNCE2], 255, 0);
	} else if (inair) {
		// airborn. do some gravity to the ball
		direction.y -= 0.075f;
	}

	if (dietime > 0 || (position.y < 0 && level < 0)) {
		// we are falling into a hole or are outside of the level
		if (dietime == 0) {
			score -= 50;
			dietime = gametime;
			snd_sfx_play(sounds[FALL], 255, 128);
		} else if (gametime - dietime > 1500) {
			dietime = 0;
			position.y = 4*3;
			position.x = -2;
			position.z = 0;
			direction.x = direction.y = direction.z = 0;
		}
	}

}

void Player::draw() {
//	sphere->_agl.z = rotation.z;
//	sphere->_agl.x = rotation.x;
//	q3dTypeQuaternion q;
//	q3dTypeVector v;

//	q3dVectorSet3f(&v, 1, 0, 0);


	q3dColorSetV(&sphere->material.color, &color);

	q3dQuaternionToMatrix(&qRot);

	q3dMatrixLoad(&_q3dMatrixCamera);
	q3dMatrixTranslate(position.x, position.y, position.z);
	q3dMatrixScale(3.0f, 3.0f, 3.0f);

	q3dMatrixApply(&_q3dMatrixTemp);
	q3dMatrixStore(&_q3dMatrixTemp);

	q3dMatrixLoad(&_q3dMatrixPerspective);
	q3dMatrixApply(&_q3dMatrixTemp);

	q3dMatrixTransformPVR(sphere->vertex, &sphere->_finalVertex[0].x, sphere->vertexLength, sizeof(pvr_vertex_t));
//	q3dMatrixIdentity();
//	q3dMatrixRotateY(rotation.y);
//	q3dQuaternionToMatrix(&qRot);
//	q3dMatrixApply(&_q3dMatrixTemp);
//	q3dMatrixStore(&_q3dMatrixTemp);
//	q3dMatrixTransformNormals(sphere->_uVertexNormal, sphere->_vertexNormal, sphere->vertexLength);

	fillerPlayers.update(sphere);
	pvr_prim(&fillerPlayers.defaultHeader, sizeof(pvr_poly_hdr_t));
	fillerPlayers.draw(sphere);
}

