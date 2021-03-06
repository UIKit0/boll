#include <math.h>
#include "credits.h"

#include "lib/Point3D.h"
#include "lib/Keyframer.h"
#include "lib/primitives.h"
#include "resources.h"

#include <plx/font.h>
#include <plx/list.h>
#include <plx/dr.h>


extern plx_fcxt_t *fcxt;


#define VECTOR Point3D

class PLANE {
public:
	float equation[4];
	float distance;
	VECTOR origin;
	VECTOR normal;

	PLANE::PLANE();
	PLANE(const VECTOR& origin, const VECTOR& normal);
	PLANE(const VECTOR& p1, const VECTOR& p2, const VECTOR& p3);
	bool isFrontFacingTo(const VECTOR& direction) const;
	double signedDistanceTo(const VECTOR& point) const;
};

class CollisionPacket {
public:
//	VECTOR eRadius; // ellipsoid radius
	// Information about the move being requested: (in R3)
//	VECTOR R3Velocity;
//	VECTOR R3Position;


	// Information about the move being requested: (in eSpace)
	VECTOR velocity;
	VECTOR normalizedVelocity;
	VECTOR basePoint;

	// Hit information
	bool foundCollision;
	double nearestDistance;
	double t;
	VECTOR intersectionPoint;
	PLANE plane;
};

PLANE::PLANE() {
}
PLANE::PLANE(const VECTOR& origin, const VECTOR& normal) {
	this->normal = normal;
	this->origin = origin;
	equation[0] = normal.x;
	equation[1] = normal.y;
	equation[2] = normal.z;
	distance = equation[3] = -(normal.x*origin.x+normal.y*origin.y +normal.z*origin.z);
}

// Construct from triangle:
PLANE::PLANE(const VECTOR& p1,const VECTOR& p2, const VECTOR& p3) {
	normal = (p2-p1).cross(p3-p1);
	normal.normalize();
	origin = p1;
	equation[0] = normal.x;
	equation[1] = normal.y;
	equation[2] = normal.z;
	equation[3] = -(normal.x*origin.x+normal.y*origin.y +normal.z*origin.z);
}

bool PLANE::isFrontFacingTo(const VECTOR& direction) const {
	double dot = normal.dot(direction);
	return (dot >= 0);
}
double PLANE::signedDistanceTo(const VECTOR& point) const {
	return (point.dot(normal)) + equation[3];
}

// typedef unsigned int uint32;

#define in(a) ((uint32&) a)

bool checkPointInTriangle(const VECTOR& point, const VECTOR& pa,const VECTOR& pb, const VECTOR& pc) {
	VECTOR e10=pb-pa;
	VECTOR e20=pc-pa;

	float a = e10.dot(e10);
	float b = e10.dot(e20);
	float c = e20.dot(e20);
	float ac_bb=(a*c)-(b*b);
	VECTOR vp(point.x-pa.x, point.y-pa.y, point.z-pa.z);

	float d = vp.dot(e10);
	float e = vp.dot(e20);
	float x = (d*c)-(e*b);
	float y = (e*a)-(d*b);
	float z = x+y-ac_bb;

	return (( in(z)& ~(in(x)|in(y)) ) & 0x80000000);
}


bool getLowestRoot(float a, float b, float c, float maxR, float* root) {
	// Check if a solution exists
	float determinant = b*b - 4.0f*a*c;
	// If determinant is negative it means no solutions.
	if (determinant < 0.0f)
		return false;

	// calculate the two roots: (if determinant == 0 then
	// x1==x2 but let s disregard that slight optimization)
	float sqrtD = fsqrt(determinant);
	float r1 = (-b - sqrtD) / (2*a);
	float r2 = (-b + sqrtD) / (2*a);

	// Sort so x1 <= x2
	if (r1 > r2) {
		float temp = r2;
		r2 = r1;
		r1 = temp;
	}

	// Get lowest root:
	if (r1 > 0 && r1 < maxR) {
		*root = r1;
		return true;
	}
	// It is possible that we want x2 - this can happen
	// if x1 < 0
	if (r2 > 0 && r2 < maxR) {
		*root = r2;
		return true;
	}

	// No (valid) solutions
	return false;
}

// Assumes: p1,p2 and p3 are given in ellisoid space:
void checkTriangle(CollisionPacket* colPackage, const VECTOR& p1,const VECTOR& p2,const VECTOR& p3) {
	// Make the plane containing this triangle.
	PLANE trianglePlane(p1,p2,p3);

	// Is triangle front-facing to the velocity vector?
	// We only check front-facing triangles
	// (your choice of course)
//	if (trianglePlane.isFrontFacingTo( colPackage->normalizedVelocity)) {
		// Get interval of plane intersection:
		double t0, t1;
		bool embeddedInPlane = false;

		// Calculate the signed distance from sphere
		// position to triangle plane
		double signedDistToTrianglePlane = trianglePlane.signedDistanceTo(colPackage->basePoint);
		// cache this as we re going to use it a few times below:
		float normalDotVelocity = trianglePlane.normal.dot(colPackage->velocity);
		// if sphere is travelling parrallel to the plane:
		if (normalDotVelocity == 0.0f) {
			if (fabs(signedDistToTrianglePlane) >= 1.0f) {
				// Sphere is not embedded in plane.
				// No collision possible:
				return;
			} else {
				// sphere is embedded in plane.
				// It intersects in the whole range [0..1]
				embeddedInPlane = true;
				t0 = 0.0; t1 = 1.0;
			}
		} else {
			// N dot D is not 0. Calculate intersection interval:
			t0 = (-1.0 - signedDistToTrianglePlane) / normalDotVelocity;
			t1 = ( 1.0 - signedDistToTrianglePlane) / normalDotVelocity;

			// Swap so t0 < t1
			if (t0 > t1) {
				double temp = t1;
				t1 = t0;
				t0 = temp;
			}

			// Check that at least one result is within range:
			if (t0 > 1.0f || t1 < 0.0f) {
				// Both t values are outside values [0,1]
				// No collision possible:
				return;
			}

			// Clamp to [0,1]
			if (t0 < 0.0) t0 = 0.0;
			if (t1 < 0.0) t1 = 0.0;
			if (t0 > 1.0) t0 = 1.0;
			if (t1 > 1.0) t1 = 1.0;
		}

		// OK, at this point we have two time values t0 and t1
		// between which the swept sphere intersects with the
		// triangle plane. If any collision is to occur it must
		// happen within this interval.
		VECTOR collisionPoint;
		bool foundCollison = false;
		float t = 1.0;

		// First we check for the easy case - collision inside
		// the triangle. If this happens it must be at time t0
		// as this is when the sphere rests on the front side
		// of the triangle plane. Note, this can only happen if
		// the sphere is not embedded in the triangle plane.
		if (!embeddedInPlane) {
			VECTOR planeIntersectionPoint = (colPackage->basePoint-trianglePlane.normal) + t0*colPackage->velocity;
			if (checkPointInTriangle(planeIntersectionPoint, p1,p2,p3)) {
				foundCollison = true;
				t = t0;
				collisionPoint = planeIntersectionPoint;
			}
		}

		// if we haven t found a collision already we ll have to
		// sweep sphere against points and edges of the triangle.
		// Note: A collision inside the triangle (the check above)
		// will always happen before a vertex or edge collision!
		// This is why we can skip the swept test if the above
		// gives a collision!
		if (foundCollison == false) {
			// some commonly used terms:
			VECTOR velocity = colPackage->velocity;
			VECTOR base = colPackage->basePoint;
			float velocitySquaredLength = velocity.squaredLength();
			float a,b,c; // Params for equation
			float newT;

			// For each vertex or edge a quadratic equation have to
			// be solved. We parameterize this equation as
			// a*t^2 + b*t + c = 0 and below we calculate the
			// parameters a,b and c for each test.
			// Check against points:
			a = velocitySquaredLength;

			// P1
			b = 2.0*(velocity.dot(base-p1));
			c = (p1-base).squaredLength() - 1.0;
			if (getLowestRoot(a,b,c, t, &newT)) {
				t = newT;
				foundCollison = true;
				collisionPoint = p1;
			}
			
			// P2
			b = 2.0*(velocity.dot(base-p2));
			c = (p2-base).squaredLength() - 1.0;
			if (getLowestRoot(a,b,c, t, &newT)) {
				t = newT;
				foundCollison = true;
				collisionPoint = p2;
			}

			// P3
			b = 2.0*(velocity.dot(base-p3));
			c = (p3-base).squaredLength() - 1.0;
			if (getLowestRoot(a,b,c, t, &newT)) {
				t = newT;
				foundCollison = true;
				collisionPoint = p3;
			}

			// Check agains edges:

			// p1 -> p2:
			VECTOR edge = p2-p1;
			VECTOR baseToVertex = p1 - base;
			float edgeSquaredLength = edge.squaredLength();
			float edgeDotVelocity = edge.dot(velocity);
			float edgeDotBaseToVertex = edge.dot(baseToVertex);

			// Calculate parameters for equation
			a = edgeSquaredLength*-velocitySquaredLength + edgeDotVelocity*edgeDotVelocity;
			b = edgeSquaredLength*(2*velocity.dot(baseToVertex))- 2.0*edgeDotVelocity*edgeDotBaseToVertex;
			c = edgeSquaredLength*(1-baseToVertex.squaredLength())+ edgeDotBaseToVertex*edgeDotBaseToVertex;

			// Does the swept sphere collide against infinite edge?
			if (getLowestRoot(a,b,c, t, &newT)) {
				// Check if intersection is within line segment:
				float f = (edgeDotVelocity*newT-edgeDotBaseToVertex)/ edgeSquaredLength;
				if (f >= 0.0 && f <= 1.0) {
					// intersection took place within segment.
					t = newT;
					foundCollison = true;
					collisionPoint = p1 + f*edge;
				}
			}

			// p2 -> p3:
			edge = p3-p2;
			baseToVertex = p2 - base;
			edgeSquaredLength = edge.squaredLength();
			edgeDotVelocity = edge.dot(velocity);
			edgeDotBaseToVertex = edge.dot(baseToVertex);
			a = edgeSquaredLength*-velocitySquaredLength + edgeDotVelocity*edgeDotVelocity;
			b = edgeSquaredLength*(2*velocity.dot(baseToVertex))- 2.0*edgeDotVelocity*edgeDotBaseToVertex;
			c = edgeSquaredLength*(1-baseToVertex.squaredLength())+ edgeDotBaseToVertex*edgeDotBaseToVertex;
			if (getLowestRoot(a,b,c, t, &newT)) {
				float f = (edgeDotVelocity*newT-edgeDotBaseToVertex)/ edgeSquaredLength;
				if (f >= 0.0 && f <= 1.0) {
					t = newT;
					foundCollison = true;
					collisionPoint = p2 + f*edge;
				}
			}

			// p3 -> p1:
			edge = p1-p3;
			baseToVertex = p3 - base;
			edgeSquaredLength = edge.squaredLength();
			edgeDotVelocity = edge.dot(velocity);
			edgeDotBaseToVertex = edge.dot(baseToVertex);
			a = edgeSquaredLength*-velocitySquaredLength + edgeDotVelocity*edgeDotVelocity;
			b = edgeSquaredLength*(2*velocity.dot(baseToVertex))- 2.0*edgeDotVelocity*edgeDotBaseToVertex;
			c = edgeSquaredLength*(1-baseToVertex.squaredLength())+ edgeDotBaseToVertex*edgeDotBaseToVertex;
			if (getLowestRoot(a,b,c, t, &newT)) {
				float f = (edgeDotVelocity*newT-edgeDotBaseToVertex)/ edgeSquaredLength;
				if (f >= 0.0 && f <= 1.0) {
					t = newT; foundCollison = true;
					collisionPoint = p3 + f*edge;
				}
			}
		}
		// Set result:
		if (foundCollison == true) {
			// distance to collision:  t  is time of collision
			float distToCollision = t*colPackage->velocity.length();
			// Does this triangle qualify for the closest hit?
			// it does if it s the first hit or the closest
			if (colPackage->foundCollision == false || distToCollision < colPackage->nearestDistance) {
				// Collision information nessesary for sliding
				colPackage->nearestDistance = distToCollision;
				colPackage->intersectionPoint = collisionPoint;
				colPackage->foundCollision = true;
				colPackage->plane.normal = trianglePlane.normal;
				colPackage->plane.equation[3] = trianglePlane.equation[3];
				colPackage->t = t;
			}
		}
//	} // if not backface
}
/*
void CharacterEntity::collideAndSlide(const VECTOR& vel, const VECTOR& gravity) {
	// Do collision detection:
	collisionPackage->R3Position = position;
	collisionPackage->R3Velocity = vel;

	// calculate position and velocity in eSpace
	VECTOR eSpacePosition = collisionPackage->R3Position/ collisionPackage->eRadius;
	VECTOR eSpaceVelocity = collisionPackage->R3Velocity/ collisionPackage->eRadius;

	// Iterate until we have our final position.
	collisionRecursionDepth = 0;
	VECTOR finalPosition = collideWithWorld(eSpacePosition, eSpaceVelocity);

	// Add gravity pull:
	// To remove gravity uncomment from here .....

	// Set the new R3 position (convert back from eSpace to R3
	collisionPackage->R3Position = finalPosition*collisionPackage->eRadius;
	collisionPackage->R3Velocity = gravity;
	eSpaceVelocity = gravity/collisionPackage->eRadius;
	collisionRecursionDepth = 0;
	finalPosition = collideWithWorld(finalPosition, eSpaceVelocity);

	// ... to here
	// Convert final result back to R3:
	finalPosition = finalPosition*collisionPackage->eRadius;

	// Move the entity (application specific function)
	MoveTo(finalPosition);
}

// Set this to match application scale..
const float unitsPerMeter = 100.0f;
VECTOR CharacterEntity::collideWithWorld(const VECTOR& pos, const VECTOR& vel) {
	// All hard-coded distances in this function is
	// scaled to fit the setting above..
	float unitScale = unitsPerMeter / 100.0f;
	float veryCloseDistance = 0.005f * unitScale;

	// do we need to worry?
	if (collisionRecursionDepth>5)
		return pos;

	// Ok, we need to worry:
	collisionPackage->velocity = vel;
	collisionPackage->normalizedVelocity = vel;
	collisionPackage->normalizedVelocity.normalize();
	collisionPackage->basePoint = pos;
	collisionPackage->foundCollision = false;

	// Check for collision (calls the collision routines)
	// Application specific!!
	world->checkCollision(collisionPackage);

	// If no collision we just move along the velocity
	if (collisionPackage->foundCollision == false) {
		return pos + vel;
	}
	// *** Collision occured ***
	// The original destination point
	VECTOR destinationPoint = pos + vel;
	VECTOR newBasePoint = pos;

	// only update if we are not already very close
	// and if so we only move very close to intersection..not
	// to the exact spot.
	if (collisionPackage->nearestDistance>=veryCloseDistance) {
		VECTOR V = vel;
		V.SetLength(collisionPackage->nearestDistanceveryCloseDistance);
		newBasePoint = collisionPackage->basePoint + V;

		// Adjust polygon intersection point (so sliding
		// plane will be unaffected by the fact that we
		// move slightly less than collision tells us)
		V.normalize();
		collisionPackage->intersectionPoint -= veryCloseDistance * V;
	}

	// Determine the sliding plane
	VECTOR slidePlaneOrigin = collisionPackage->intersectionPoint;
	VECTOR slidePlaneNormal = newBasePoint-collisionPackage->intersectionPoint;
	slidePlaneNormal.normalize();

	PLANE slidingPlane(slidePlaneOrigin,slidePlaneNormal);

	// Again, sorry about formatting.. but look carefully ;)
	VECTOR newDestinationPoint = destinationPoint - slidingPlane.signedDistanceTo(destinationPoint)* slidePlaneNormal;

	// Generate the slide vector, which will become our new
	// velocity vector for the next iteration
	VECTOR newVelocityVector = newDestinationPoint - collisionPackage->intersectionPoint;

	// Recurse:
	// dont recurse if the new velocity is very small
	if (newVelocityVector.length() < veryCloseDistance) {
		return newBasePoint;
	}
	collisionRecursionDepth++;
	return collideWithWorld(newBasePoint,newVelocityVector);
}
*/

#define LINENUM 178
static int width[LINENUM];

static float fontSize = 24;
static char cred_text[LINENUM][45] = {
	"== Concept ==",
	"Fredrik Ehnbom",
	"Andreas Ehnbom",
	"Andreas Hedlund",
	"",
	"== Programming ==",
	"Fredrik Ehnbom",
	"",
	"== Music composed, ==",
	"== arranged and produced by ==",
	"Andreas Hedlund",
	"",
	"== Mixing and mastering ==",
	"Andreas Hedlund",
	"Johan Olkerud",
	"",
	"== Sound effects ==",
	"Andreas Hedlund",
	"",
	"== 3d graphics and textures ==",
	"Fredrik Ehnbom",
	"",
	"This has been the result of",
	"around 50 days of work.",
	"",
	"The project started at January 23rd",
	"and was submitted to the dream-on",
	"competition at March 17th",
	"(the same day as the deadline..)",
	"",
	"",
	"",
	"Fredrik on the keys now.",
	"",
	"This has been the busiest period in my",
	"life as of yet, and I am pretty amazed",
	"by the fact that I managed to work on",
	"this game, study for school and attend",
	"my wing chun training with neither of",
	"thesea suffering too much.",
	"",
	"I just wish that I had found out about",
	"this competition earlier. Then maybe the",
	"game would have been fully completed",
	"by now.",
	"",
	"",
	"But then again... maybe not ;)",
	"",
	"",
	"",
	"It's just so hard to come up with an",
	"idea for a game that's acctually fun",
	"to play...",
	"",
	"What seems to be a good game on paper",
	"might very well not be fun at all in",
	"reality.",
	"",
	"We had so many ideas when we first",
	"started to brainstorm about this games",
	"concept.",
	"",
	"Most of them have been thrown away,",
	"others where saved until \"later\"",
	"because of the time constraints, but",
	"very few of them are acctually in this",
	"game now.",
	"",
	"But really, creating a game that is fun",
	"to play and original is NOT very easy at",
	"all.",
	"",
	"My respect for professional game",
	"developers have definately grown over",
	"the time that this game has been",
	"developed.",
	"",
	"",
	"",
	"Acctually this is the first \"real\" game",
	"that I've ever coded.",
	"Before this I've only made \"snake\"",
	"(but I did it twice! ;))",
	"",
	"Not that bad for a first timer eh?",
	"",
	"",
	"",
	"Handing over the keyboard to",
	"our musician now:",
	"",
	"",
	"",
	"Boll from my perspective,",
	"",
	"When Fredrik came to me with the",
	"\"boll\" idea, I knew it would",
	"be a hit. At least it was in my",
	"mind. Later it turned out not",
	"to be as easy as I'd thought.",
	"We didn't have time to finish",
	"all our ideas, but I hope that",
	"we can implement a majority of",
	"them in the future.",
	"",
	"",
	"Anyway, we have a finished game",
	"that we are not ashamed of,a game",
	"that is playable and fun, that",
	"was created in a very short time.",
	"",
	"",
	"Now, my part of the work was",
	"mainly based on the sounds",
	"and the soundtracks.",
	"",
	"",
	"The soundtracks was a great",
	"experience. It began with the",
	"\"menu\"-tune, and then I",
	"completly changed the theme",
	"of the game into a more jazzy",
	"mood, but the menu fitted well",
	"so I kept it. Inspiration came",
	"from a lot of sources, but the",
	"event that started it all was",
	"the jazz concert with",
	"Nils Landgren (a swedish",
	"trombonist). Also, inspiration",
	"from the 80:th, and inspiration",
	"from my friend Henrik Joses",
	"songs. Thanks to Johan Olkerud,",
	"who helped with the mastering",
	"of the soundtrack",
	"",
	"",
	"It was the first time I've made",
	"sound effects and I think it",
	"turned out pretty well. Although,",
	"my passion is composing music,",
	"the effect work was useful",
	"learning. Almost all the effects",
	"were modulated with my Nord Lead",
	"synthesizer, and the others from",
	"my Korg Triton.",
	"",
	"/Andreas Hedlund",
	"",
	"",
	"",
	"",
	"This game would not have been possible",
	"without all the people writing code",
	"(and the very few who write docs..)",
	"for the dreamcast video gaming system.",
	"Therefore I would like to thank the",
	"following people (in no special order):",
	"",
	"Marcus Comstedt",
	"Mikael Kalms",
	"Dan Potter",
	"Lars Olsson",
	"Yamato",
	"",
	"Of course, this list could be a lot",
	"longer, but these are the names that",
	"came to my mind",
	"",
	"",
	"",
	"",
	"and last but not least",
	"THANK YOU",
	"(for playing this game)",
	"",
	"",
	"*hugs*",
};

static uint64 startTime = 0;

static KeyFramer kFrame;
static KeyFrame frames[4];

static q3dTypeVertex world_coordinates[8];

Credits::Credits() {
	for (int i = 0; i < LINENUM; i++) {
		float l, u, d, r;
		plx_fcxt_str_metrics(fcxt, cred_text[i], &l, &u, &r, &d);
		width[i] = 640 / 2 - (int) ((l+r) / 2);
	}
	frames[0].continuity = frames[3].continuity = 1.0f;
	frames[1].continuity = frames[2].continuity = 0.0f;
	frames[1].bias = frames[2].bias = 1.0f;
	frames[0].set(0, 480+fontSize, 0); frames[0].time = 0;
	frames[1].set(0.8, 320+fontSize, 0); frames[1].time = 0.3125;
	frames[2].set(0.8, 160+fontSize, 0); frames[2].time = 8.75;
	frames[3].set(0, 0-fontSize, 0); frames[3].time = 9.0625;
	kFrame.generate(frames, 4);

	sphere = generateSphere(6, 6);
	q3dPolyhedronCompile(sphere);

	cube = generateCube(10);
	q3dPolyhedronCompile(cube);

	q3dFillerStandardInit(&sphereFiller);
	sphere->material.header = sphereFiller.defaultHeader;

	q3dFillerStandardInit(&cubeFiller);
	pvr_poly_cxt_col(&cubeFiller.defaultCxt, PVR_LIST_TR_POLY);
	cubeFiller.defaultCxt.gen.culling = PVR_CULLING_NONE;
	pvr_poly_compile(&cubeFiller.defaultHeader, &cubeFiller.defaultCxt);
	cube->material.header = cubeFiller.defaultHeader;

	q3dCameraInit(&cam);
	cam._pos.z = -5;

	torus = generateTorus(20, 2);
	q3dPolyhedronCompile(torus);

	q3dFillerEnvironmentInit(&torus1Filler);
	torus1Filler.defaultCxt = loadImage("env.png", PVR_LIST_OP_POLY);
	pvr_poly_compile(&torus1Filler.defaultHeader, &torus1Filler.defaultCxt);
}

Credits::~Credits() {
	printf("polyhedron free cube: ...");
	q3dPolyhedronFree(cube);
	printf("done!\nfree cube: ...");
	free(cube);
	printf("done!\n");

	printf("polyhedronfree sphere: ..."); 
	q3dPolyhedronFree(sphere);
	printf("done!\nfree sphere: ...");
	free(sphere);
	printf("done!\n");

	q3dPolyhedronFree(torus);
	free(torus);
}

static Point3D velocity;
void Credits::run() {
	static Point3D key;

	cddaPlay(CREDITTRACK);
	q3dMatrixInit();
	plx_fcxt_setsize(fcxt, fontSize);

	Point3D gravity(0,-0.1,0);

	startTime = timer_ms_gettime64();

	uint32 last;
	maple_device_t *dev = maple_enum_dev(0, 0);
	if (dev != NULL && dev->info.functions & MAPLE_FUNC_CONTROLLER) {
		cont_state_t* st = (cont_state_t*) maple_dev_status(dev);
		last = st->buttons;
	}

	while (true) {
		dev = maple_enum_dev(0, 0);
		if (dev != NULL && dev->info.functions & MAPLE_FUNC_CONTROLLER) {
			cont_state_t* st = (cont_state_t*) maple_dev_status(dev);
			if (st->buttons != last) {
				if (st->buttons & CONT_START || st->buttons & CONT_A) {
					return;
				}
			}

			last = st->buttons;
		}

		float time = (timer_ms_gettime64() - startTime) / 1000.0f;
		if (time - 5 - LINENUM*1.25 > 10) {
			startTime = timer_ms_gettime64();
		}

		// begin rendering
		pvr_wait_ready();
		pvr_scene_begin();

		pvr_list_begin(PVR_LIST_OP_POLY);

		plane.draw();
//		q3dPolyhedronPaint(sphere, &cam, &sphereFiller);

		float time2 = timer_ms_gettime64() / 1000.0f;
		torus->material.header = torus1Filler.defaultHeader;

		float a = time2 * 0.5 * 1.2 * 0.5;
		static q3dTypeQuaternion qx;
		qx.w = cos(a);
		qx.x = sin(a);
		qx.y = qx.z = 0;

		a = time2 * 0.5 * 1.1 * 0.5;
		static q3dTypeQuaternion qz;
		qz.w = cos(a);
		qz.x = qz.y = 0;
		qz.z = sin(a);

		q3dQuaternionNormalize(&qx);
		q3dQuaternionNormalize(&qz);

		q3dQuaternionMul(&qx, &qz);

		q3dColorSet3f(&torus->material.color, 1.0f, 1.0f,1.0f);
		q3dQuaternionToMatrix(&qx);

		q3dMatrixLoad(&_q3dMatrixPerspective);
//		q3dMatrixApply(&_q3dMatrixCamera);
		q3dMatrixTranslate(0, 0, 5/*position.z*/);

		q3dMatrixApply(&_q3dMatrixTemp);
//		q3dMatrixStore(&_q3dMatrixTemp);
//		q3dMatrixApply(&_q3dMatrixTemp);

		q3dMatrixTransformPVR(torus->vertex, &torus->_finalVertex[0].x, torus->vertexLength, sizeof(pvr_vertex_t));

		q3dMatrixTransformNormals(torus->_uVertexNormal, torus->_vertexNormal, torus->vertexLength);

		torus1Filler.update(torus);
		pvr_prim(&torus1Filler.defaultHeader, sizeof(pvr_poly_hdr_t));
		torus1Filler.draw(torus);

		//		q3dPolyhedronPaint(torus, &cam, &torus1Filler);

		pvr_list_finish();

		pvr_list_begin(PVR_LIST_TR_POLY);

		// draw the cube
		q3dColorSet4f(&cube->material.color, 1.0f, 1.0f, 1.0f, 0.5);
//		q3dPolyhedronPaint(cube, &cam, &cubeFiller);

		// draw credits text
		plx_fcxt_begin(fcxt);

		for (int i = 0; i < LINENUM; i++) {
			kFrame.getKey(frames, 4, &key, time - 5 - i*1.25);
			float y = key.y; //(int) (i * fontSize + 320 - time * 34);
			if (y < 0) continue;
			if (y > 480) break;

			plx_fcxt_setcolor4f(fcxt, key.x, 1, 1, 1);
			plx_fcxt_setpos(fcxt, width[i], y, 2);
			plx_fcxt_draw(fcxt, cred_text[i]);
		}

		plx_fcxt_end(fcxt);

		pvr_list_finish();

		pvr_scene_finish();
	}
}
