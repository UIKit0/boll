#include <string.h>
#include "game.h"
#include "lib/primitives.h"

#include <plx/font.h>
#include <plx/list.h>
#include <plx/dr.h>

static point_t w;
extern uint32 polysent;
extern uint32 vertextest;


q3dTypePolyhedron *sphere;
q3dTypePolyhedron *scorePolyhedron;
q3dTypeFiller fillerPlayers;
q3dTypeFiller fillerLevel;
q3dTypeFiller fillerScore;

q3dTypeMatrix screen_matrix __attribute__((aligned(32))) = {
    { 640/4.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 480 / 4.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};

Game::Game() {
	q3dMatrixInit();

	sphere = generateSphere(6, 6);
	scorePolyhedron = generateSphere(3, 3);

	q3dFillerCellInit(&fillerPlayers);
	fillerPlayers.defaultCxt.gen.clip_mode = PVR_USERCLIP_INSIDE;
	fillerPlayers.defaultCxt.gen.fog_type = PVR_FOG_TABLE;
	pvr_poly_compile(&fillerPlayers.defaultHeader, &fillerPlayers.defaultCxt);

	q3dFillerTextureInit(&fillerLevel);
	fillerLevel.defaultCxt = loadImage("buzz.png", PVR_LIST_OP_POLY);
	fillerLevel.defaultCxt.gen.clip_mode = PVR_USERCLIP_INSIDE;
	fillerLevel.defaultCxt.gen.fog_type = PVR_FOG_TABLE;
	pvr_poly_compile(&fillerLevel.defaultHeader, &fillerLevel.defaultCxt);

	q3dFillerStandardInit(&fillerScore);
	fillerScore.defaultCxt.gen.clip_mode = PVR_USERCLIP_INSIDE;
//	fillerScore.defaultCxt.gen.fog_type = PVR_FOG_TABLE;
	pvr_poly_compile(&fillerScore.defaultHeader, &fillerScore.defaultCxt);

	q3dCameraInit(&cam);
	cam._pos.y = 5;
	cam._pos.z = -5;

	player = new Player[4];
	for (int i = 0; i < 4; i++)
		player[i].setController(i);

	player[0].position.set( 0,  0,  +2);
	player[1].position.set( 0,  10, +2);
	player[2].position.set(-2,  2, -2);
	player[3].position.set(+2,  2, -2);

	pvr_poly_cxt_t cxt;
	pvr_poly_cxt_col(
		&cxt,
		PVR_LIST_OP_POLY
	);

	pvr_poly_compile(&crossHeader, &cxt);

	w.x = 20;
	w.y = 32;
	w.z = 100;
}

Game::~Game() {
	printf("free sphere: ...");
	q3dPolyhedronFree(sphere);
	printf("done!\nfree sphere2: ...");
	free(sphere);
	printf("done!\ndelete[] player: ...");
	delete[] player;
	printf("done!\n");
}

extern bool done;
extern uint32 qtime;
void Game::run() {
	while (!done/*true*/) {
		// TODO: check for exit.. ?
		qtime = timer_ms_gettime64();
		update();
		draw();
	}
}

void Game::update() {
	for (int i = 0; i < 4; i++)
		player[i].update(this);

	for (int i = 0; i < MAX_SCORE_NUM; i++) {
		score[i].update(this);
	}
	level.update();
}

static pvr_poly_hdr_t user_clip = {
        PVR_CMD_USERCLIP, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000
};
extern matrix_t projection_matrix;
void Game::draw() {

	static char buf[256];
	polysent = 0;
	vertextest = 0;

	// begin render with TA
	pvr_wait_ready();
	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_OP_POLY);
	uint64 start = timer_ms_gettime64();

	// draw..
	for (int i = 0; i < 4; i++) {
		q3dMatrixIdentity();
		q3dMatrixTranslate(0,-4.5f-player[i].camheight-player[i].camadd, player[i].camzoom /*10*/);
		q3dMatrixRotateX(player[i].camagl);
		q3dMatrixRotateY(player[i].rotation.y);
		q3dMatrixTranslate(-player[i].position.x, 0/*-player[i].position.y*/,-player[i].position.z);

		q3dMatrixStore(&_q3dMatrixCamera);
		if (i == 0) {
			// upper left
			screen_matrix[3][0] = 1 * 640 / 4.0f;
			screen_matrix[3][1] = 1 * 480 / 4.0f;
			user_clip.d1 = 0;
			user_clip.d2 = 0;
			user_clip.d3 = (640/2)/32-1;
			user_clip.d4 = (480/2)/32-1;
		} else if (i == 1) {
			// upper right
			screen_matrix[3][0] = 640 * 3 / 4.0f;
			screen_matrix[3][1] = 480 * 1 / 4.0f;
			user_clip.d1 = (640/2)/32;
			user_clip.d2 = 0;
			user_clip.d3 = (4*640/4)/32-1;
			user_clip.d4 = (480/2)/32-1;
		} else if (i == 2) {
			// lower left
			screen_matrix[3][0] = 1 * 640 / 4.0f;
			screen_matrix[3][1] = 3 * 480 / 4.0f;
			user_clip.d1 = 0;
			user_clip.d2 = (480/2)/32;
			user_clip.d3 = (640/2)/32-1;
			user_clip.d4 = (4*480/4)/32-2;
		} else {
			// lower right
			screen_matrix[3][0] = 3 * 640 / 4.0f;
			screen_matrix[3][1] = 3 * 480 / 4.0f;
			user_clip.d1 = (640/2)/32;
			user_clip.d2 = (480/2)/32;
			user_clip.d3 = (4*640/4)/32-1;
			user_clip.d4 = (4*480/4)/32-2;
		}
		pvr_prim(&user_clip, sizeof(pvr_poly_hdr_t));
		q3dMatrixLoad(&screen_matrix);
		q3dMatrixApply(&projection_matrix);
		q3dMatrixStore(&_q3dMatrixPerspective);

		q3dTypeVertex vert;
		for (int j = 0; j < 4; j++) {
			q3dMatrixLoad(&_q3dMatrixCamera);
			q3dVertexSet3f(&vert, player[j].position.x, player[j].position.y, player[j].position.z);
			mat_trans_single3(vert.x, vert.y, vert.z);
			if (vert.z < 3)
				continue;
			if (vert.z > 100)
				continue;
			if (vert.x > vert.z + 4)
				continue;
			if (vert.x < -vert.z-4)
				continue;

			player[j].draw();
		}
		for (int j = 0; j < MAX_SCORE_NUM; j++) {
			q3dMatrixLoad(&_q3dMatrixCamera);
			q3dVertexSet3f(&vert, score[j].position.x, score[j].position.y, score[j].position.z);
			mat_trans_single3(vert.x, vert.y, vert.z);

			// test if outside viewing frustum
			if (vert.z < 0)
				continue;
			if (vert.z > 100)
				continue;
			if (vert.x > vert.z + 2)
				continue;
			if (vert.x < -vert.z-2)
				continue;

			score[j].draw();
		}

		q3dMatrixIdentity();
		q3dMatrixRotateY(player[i].rotation.y);
		q3dMatrixStore(&_q3dMatrixTemp);
//		if (i == 1) {
			level.draw();
//		}
	}

	uint64 end = timer_ms_gettime64();

	// commit cross
	pvr_prim(&crossHeader, sizeof(pvr_poly_hdr_t));
	pvr_vertex_t vert;
	vert.argb = PVR_PACK_COLOR(1.0f, 0.0f, 0.0f, 0.0f);
	vert.z = 1 / 0.01f;

	// vertical line
	vert.flags = PVR_CMD_VERTEX;
	vert.x = 640/2-1; vert.y = 480-32;
	pvr_prim(&vert, sizeof(pvr_vertex_t));

	/*vert.x = 640/2-1;*/ vert.y = 0;
	pvr_prim(&vert, sizeof(pvr_vertex_t));

	vert.x = 640/2+1; vert.y = 480-32;
	pvr_prim(&vert, sizeof(pvr_vertex_t));

	vert.flags = PVR_CMD_VERTEX_EOL;
	/*vert.x = 448+xadd;*/ vert.y = 0;
	pvr_prim(&vert, sizeof(pvr_vertex_t));

	// horizontal line
	vert.flags = PVR_CMD_VERTEX;
	vert.x = 0; vert.y = (480-32)/2+1;
	pvr_prim(&vert, sizeof(pvr_vertex_t));

	/*vert.x = 640/2-1;*/ vert.y = (480-32)/2-1;
	pvr_prim(&vert, sizeof(pvr_vertex_t));

	vert.x = 640; vert.y = (480-32)/2+1;
	pvr_prim(&vert, sizeof(pvr_vertex_t));

	vert.flags = PVR_CMD_VERTEX_EOL;
	/*vert.x = 448+xadd;*/ vert.y = (480-32)/2-1;
	pvr_prim(&vert, sizeof(pvr_vertex_t));

	// todo: draw health, score, frags, etc-display

	pvr_list_finish();

	pvr_list_begin(PVR_LIST_TR_POLY);

#ifdef BETA
	w.y = 32;
	plx_fcxt_begin(fcxt);

	w.y += 32;
	plx_fcxt_setpos_pnt(fcxt, &w);
	sprintf(buf, "time: %d", end - start);
	plx_fcxt_draw(fcxt, buf);

	w.y += 32;
	plx_fcxt_setpos_pnt(fcxt, &w);
	sprintf(buf, "polygons: %d", polysent);
	plx_fcxt_draw(fcxt, buf);

	w.y += 32;
	plx_fcxt_setpos_pnt(fcxt, &w);
	sprintf(buf, "score: %d", player[0].score);
	plx_fcxt_draw(fcxt, buf);
/*
	w.y += 32;
	plx_fcxt_setpos_pnt(fcxt, &w);
	sprintf(buf, "tests: %d", vertextest);
	plx_fcxt_draw(fcxt, buf);

	w.y += 32;
	plx_fcxt_setpos_pnt(fcxt, &w);
	sprintf(buf, "pos: (%f, %f, %f)", player[0].position.x, player[0].position.y, player[0].position.z);
	plx_fcxt_draw(fcxt, buf);
	w.y += 32;
	plx_fcxt_setpos_pnt(fcxt, &w);
	sprintf(buf, "rot: (%f, %f, %f)", player[0].rotation.x, player[0].rotation.y, player[0].rotation.z);
	plx_fcxt_draw(fcxt, buf);
*/
	plx_fcxt_end(fcxt);
#endif

	pvr_list_finish();

	pvr_scene_finish();
}

