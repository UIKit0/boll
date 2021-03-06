#include "plane.h"
#include "../lib/Point3D.h"
#include "../resources.h"

#include <stdio.h>
#include <math.h>

#define WIDTH 640
#define HEIGHT 480
#define SIZE WIDTH*HEIGHT

#define idiv16(a, b) (long) (((a) << 16) / (b))
#define i16(a) (long) ((a) << 16)

#define imul16(a, b) (long) (((a) * (b)) >> 16)


class Point {
public:
	float u;
	float v;
	float shade;
};

class Matrix {
private:
	float xx, xy, xz, xo;
	float yx, yy, yz, yo;
	float zx, zy, zz, zo;
public:
	Matrix() {
		identity();
	}

	/**
	 * reset
	 */
	void identity() {
		xx = 1; xy = 0; xz = 0; xo = 0;
		yx = 0; yy = 1; yz = 0; yo = 0;
		zx = 0; zy = 0; zz = 1; zo = 0;
	}

	/**
	 * Rotate x
	 * <pre>
	 *
	 *           [ 1   0      0   0 ]
	 *           [ 0 cosax -sinax 0 ]
	 * Rx(ax) =  [ 0 sinax  cosax 0 ]
	 *           [ 0   0      0   1 ]
	 * </pre>
	 */
	void rotateX(float ax) {
		float ct = (float) fcos(ax);
		float st = (float) fsin(ax);

		float Nyx = (yx * ct + zx * st);
		float Nyy = (yy * ct + zy * st);
		float Nyz = (yz * ct + zz * st);
		float Nyo = (yo * ct + zo * st);

		float Nzx = (zx * ct - yx * st);
		float Nzy = (zy * ct - yy * st);
		float Nzz = (zz * ct - yz * st);
		float Nzo = (zo * ct - yo * st);

		yx = Nyx; yy = Nyy; yz = Nyz; yo = Nyo;
		zx = Nzx; zy = Nzy; zz = Nzz; zo = Nzo;
	}

	/**
	 * Rotate y
	 * <pre>
	 *
	 *           [ cosay 0 sinay 0 ]
	 *           [   0   1   0   0 ]
	 * Ry(ay) =  [-sinay 0 cosay 0 ]
	 *           [   0   0   0   1 ]
	 * </pre>
	 */
	void rotateY(float ay) {
		float ct = (float) fcos(ay);
		float st = (float) fsin(ay);

		float Nxx = (xx * ct + zx * st);
		float Nxy = (xy * ct + zy * st);
		float Nxz = (xz * ct + zz * st);
		float Nxo = (xo * ct + zo * st);

		float Nzx = (zx * ct - xx * st);
		float Nzy = (zy * ct - xy * st);
		float Nzz = (zz * ct - xz * st);
		float Nzo = (zo * ct - xo * st);

		xx = Nxx; xy = Nxy; xz = Nxz; xo = Nxo;
		zx = Nzx; zy = Nzy; zz = Nzz; zo = Nzo;
	}

	/**
	 * Rotate z
	 * <pre>
	 *
	 *           [ cosaz -sinaz 0 0 ]
	 *           [ sinaz  cosaz 0 0 ]
	 * Rz(az) =  [   0      0   1 0 ]
	 *           [   0      0   0 1 ]
	 * </pre>
	 */
	void rotateZ(float az) {
		float ct = (float) fcos(az);
		float st = (float) fsin(az);

		float Nxx = (xx * ct - yx * st);
		float Nxy = (xy * ct - yy * st);
		float Nxz = (xz * ct - yz * st);
		float Nxo = (xo * ct - yo * st);

		float Nyx = (yx * ct + xx * st);
		float Nyy = (yy * ct + xy * st);
		float Nyz = (yz * ct + xz * st);
		float Nyo = (yo * ct + xo * st);

		xx = Nxx; xy = Nxy; xz = Nxz; xo = Nxo;
		yx = Nyx; yy = Nyy; yz = Nyz; yo = Nyo;
	}

	void transform(Point3D &v) {
		float x = v.x;
		float y = v.y;
		float z = v.z;

		v.x = x * xx + y * xy + z * xz;
		v.y = x * yx + y * yy + z * yz;
		v.z = x * zx + y * zy + z * zz;

	}
};

static Matrix mat;
static Point3D Origin(0,0, -256);

inline static float sign(float a) {
	return a < 0 ? -1 : a > 0 ? 1 : 0;
}

inline static void getUV_plane(int x, int y, Point &p) {
	Point3D Direction(x - WIDTH/2, y - HEIGHT/2, 256);
	mat.transform(Direction);
	if (Direction.y > -20 && Direction.y < 20) {
		p.shade = 0;
		return;
	}

	float t = (sign(Direction.y) * 1024 - Origin.y) / Direction.y;
	// finnally the intersection
	Point3D Intersection(0,0,0);
	Intersection.x = Origin.x + t*Direction.x;
	Intersection.y = Origin.y + t*Direction.y;
	Intersection.z = Origin.z + t*Direction.z;

 	p.u = Intersection.x * 0.2 / 255.0f;
	p.v = Intersection.z * 0.2 / 255.0f;
	p.shade = 1 +  1 - (t/20.0f);
	p.shade = p.shade < 0 ? 0 : p.shade > 1 ? 1 : p.shade;
}


#define BLOCKSIZE 8

Plane::Plane() {
	bw = (WIDTH / BLOCKSIZE)+1;
	bh = (HEIGHT / BLOCKSIZE)+1;

	p = new Point[bw * bh];

	pvr_poly_cxt_t cxt = loadImage("plane.png", PVR_LIST_OP_POLY);
//	cxt.txr.uv_clamp = PVR_UVCLAMP_UV;
//	cxt.txr.filter = PVR_FILTER_NONE;
	cxt.depth.comparison = PVR_DEPTHCMP_ALWAYS;
	cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
	pvr_poly_compile(&header, &cxt);
}

Plane::~Plane() {
	delete[] p;
	// TODO: free texture
}
void Plane::draw() {
	float time = (float) (timer_ms_gettime64() / 1000.0f);
	time *= 0.25f;


	Origin.y = sin(time) * 10;
//		Origin.x = sin(time*0.67) * 10;
//		Origin.z = -2048;
	Origin.z = -time * 1000.0f * 10.0f;
	mat.identity();
	mat.rotateX(fsin(time*1.032f));
	mat.rotateY(time);
	mat.rotateZ(time*1.2f);
//		mat.rotateY(2*3.141592/16);

	for (int y = 0; y < bh; y ++) {
		for (int x = 0; x < bw; x ++) {
//				getUV(x*BLOCKSIZE, y*BLOCKSIZE, p[y * bw + x]);
			getUV_plane(x*BLOCKSIZE, y*BLOCKSIZE, p[y * bw + x]);
		}
	}

	pvr_prim(&header, sizeof(pvr_poly_hdr_t));

	pvr_dr_state_t dr_state;
	pvr_dr_init(dr_state);
	pvr_vertex_t *vert;

	float z = 1 / 1000.0f;

	for (int y = 0; y < bh-2; y ++) {
		int ybw = y*bw;
		int y1bw = (y+1)*bw;
		int y1BLOCKSIZE = (y+1)*BLOCKSIZE;
		int yBLOCKSIZE = y*BLOCKSIZE;

		for (int x = 0; x < bw-1; x ++) {
			vert = pvr_dr_target(dr_state);
			vert->flags = PVR_CMD_VERTEX;
			vert->z = z;

			vert->u = p[y1bw+x].u; vert->v = p[y1bw+x].v;
			vert->x = x * BLOCKSIZE; vert->y = y1BLOCKSIZE;
			vert->argb = PVR_PACK_COLOR(1.0f, p[y1bw+x].shade, p[y1bw+x].shade, p[y1bw+x].shade);
			pvr_dr_commit(vert);

			vert = pvr_dr_target(dr_state);
			vert->flags = PVR_CMD_VERTEX;
			vert->z = z;
			vert->u = p[ybw+x].u; vert->v = p[ybw+x].v;
			vert->x = x * BLOCKSIZE; vert->y = yBLOCKSIZE;
			vert->argb = PVR_PACK_COLOR(1.0f, p[ybw+x].shade, p[ybw+x].shade, p[ybw+x].shade);
			pvr_dr_commit(vert);
		}
		vert = pvr_dr_target(dr_state);
		vert->flags = PVR_CMD_VERTEX;
		vert->z = z;
		vert->u = p[y1bw+(bw-1)].u; vert->v = p[y1bw+(bw-1)].v;
		vert->x = (bw-1) * BLOCKSIZE; vert->y = (y+1)*BLOCKSIZE;
		vert->argb = PVR_PACK_COLOR(1.0f, p[y1bw+(bw-1)].shade, p[y1bw+(bw-1)].shade, p[y1bw+(bw-1)].shade);
		pvr_dr_commit(vert);

		vert = pvr_dr_target(dr_state);
		vert->flags = PVR_CMD_VERTEX_EOL;
		vert->z = z;
		vert->u = p[ybw+(bw-1)].u; vert->v = p[ybw+(bw-1)].v;
		vert->x = (bw-1) * BLOCKSIZE; vert->y = y*BLOCKSIZE;
		vert->argb = PVR_PACK_COLOR(1.0f, p[ybw+(bw-1)].shade, p[ybw+(bw-1)].shade, p[ybw+(bw-1)].shade);
		pvr_dr_commit(vert);
	}
}
