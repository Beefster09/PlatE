
#include "sprite.h"
#include "hitbox.h"
#include "transform.h"
#include <SDL_gpu.h>
#include <cstring>
#include <cassert>

Hitbox::Hitbox(Hitbox& other) {
	type = other.type;
	switch (type) {
	case BOX:
		box = other.box;
		return;
	case CIRCLE:
		circle = other.circle;
		return;
	case LINE:
	case ONEWAY:
		line = other.line;
		return;
	case POLYGON:
		polygon = other.polygon;
		return;
	case COMPOSITE:
		composite = other.composite;
		return;
	}
}

Array2D<bool> CollisionType::table = Array2D<bool>();
Array<const CollisionType> CollisionType::types = Array<const CollisionType>();

const CollisionType* CollisionType::by_name(const char* name) {
	for (const CollisionType& cgroup : CollisionType::types) {
		if (strcmp(name, cgroup.name) == 0) return &cgroup;
	}
	printf("Warning: '%s' is not a valid CollisionType.\n", name);
	return nullptr;
}

void CollisionType::init(const char* config_file) {
	int n_cgroups = 5;

	// TODO: count user types

	CollisionType* types = new CollisionType[n_cgroups];

	// init intrinsics
	types[0] = CollisionType("Entity",  0, { 127, 127, 127 });
	types[1] = CollisionType("Level",   1, {  32, 192,   0 });
	types[2] = CollisionType("Damage",  2, { 255,   0,   0 });
	types[3] = CollisionType("Hurtbox", 3, { 255, 255,  64 });
	types[4] = CollisionType("Block",   4, { 192,   0, 255 });

	// TODO: parse user types

	CollisionType::types = Array<const CollisionType>(types, n_cgroups);

	table = Array2D<bool>(n_cgroups, n_cgroups);
	table.clear();

	table.set(0, 0); // Entity -> Entity
	table.set(0, 1); // Entity -> Level
	table.set(2, 3); // Damage -> Hurtbox
	table.set(2, 4); // Damage -> Block

	// TODO: parse user interactions
}

void render_hitbox(GPU_Target* context, const Transform& tx, const Hitbox& hitbox, const SDL_Color& color) {
	SDL_Color stroke, fill;
	stroke = color;
	stroke.a = 192;
	fill = color;
	fill.a = 64;

	switch (hitbox.type) {
	case Hitbox::BOX:
	{
		if (tx.is_rect_invariant()) {
			AABB box = tx * hitbox.box;

			GPU_RectangleFilled(context, box.left, box.top, box.right, box.bottom, fill);
			GPU_Rectangle(context, box.left, box.top, box.right, box.bottom, stroke);
		}
		else {
			Vector2 topleft = tx * Vector2{ hitbox.box.left, hitbox.box.top };
			Vector2 bottomright = tx * Vector2{ hitbox.box.right, hitbox.box.bottom };
			float vertices[8] = {
				topleft.x, topleft.y,
				bottomright.x, topleft.y,
				bottomright.x, bottomright.y,
				topleft.x, bottomright.y
			};

			GPU_PolygonFilled(context, 4, vertices, fill);
			GPU_Polygon(context, 4, vertices, stroke);
		}
	}
		break;
	case Hitbox::CIRCLE:
	{
		Vector2 center = tx * hitbox.circle.center;
		Vector2 bottom = tx * (hitbox.circle.center + Vector2{0.f, hitbox.circle.radius});
		float radius = distance(bottom, center);

		GPU_CircleFilled(context, center.x, center.y, radius, fill);
		GPU_Circle(context, center.x, center.y, radius, stroke);
	}
		break;
	case Hitbox::LINE:
	case Hitbox::ONEWAY:
	{
		Vector2 p1 = (tx * hitbox.line.p1).floor();
		Vector2 p2 = (tx * hitbox.line.p2).floor();

		GPU_Line(context, p1.x, p1.y, p2.x, p2.y, stroke);

		if (hitbox.type == Hitbox::ONEWAY) {
			Vector2 tag = (p2 - p1).normalized().rotated90CW() * 3.f;
			for (float t = 0.25f; t < 1.f; t += 0.25f) {
				Vector2 tagp1 = lerp(p1, p2, t);

				GPU_Line(context, tagp1.x, tagp1.y, tagp1.x + tag.x, tagp1.y + tag.y, stroke);
			}
		}
	}
		break;
	case Hitbox::POLYGON:
	{
		Vector2 vertexBuf[32];
		int i = 0;
		for (auto vertex : hitbox.polygon.vertices) {
			vertexBuf[i++] = tx * vertex;
		}

		GPU_PolygonFilled(context, hitbox.polygon.vertices.size(), (float*)vertexBuf, fill);
		GPU_Polygon(context, hitbox.polygon.vertices.size(), (float*)vertexBuf, stroke);
	}
		break;
	case Hitbox::COMPOSITE:
		for (const Hitbox& subhit : hitbox.composite.hitboxes) {
			render_hitbox(context, tx, subhit, color);
		}
		break;
	}
}


void render_colliders(GPU_Target* context, const Transform& tx, const CollisionData& collision) {
	for (const Collider& collider: collision) {
		render_hitbox(context, tx, collider.hitbox, collider.type->color);
	}
}

static bool box_box_test(AABB a, AABB b);
static bool box_circle_test(AABB box, Circle circle);
static bool box_line_test(AABB box, Line line);
static bool circle_circle_test(Circle a, Circle b);
static bool circle_line_test(Circle circle, Line line);
static bool line_line_test(Line a, Line b);
static bool poly_poly_test(Array<Point2> a, Array<Point2> b);
static bool poly_line_test(Array<Point2> poly, Line line);
static bool poly_circle_test(Array<Point2> poly, Circle radius);

bool hitboxes_overlap(
	const Hitbox* a, const Transform& aTx, Vector2 aDis, // aDis and bDis are the displacement since last frame
	const Hitbox* b, const Transform& bTx, Vector2 bDis  // They are only needed for checking oneway collisions
) {
	if (a->type == Hitbox::ONEWAY) {
		if (b->type == Hitbox::ONEWAY) return false; // One-ways don't collide with each other
		else return hitboxes_overlap(b, bTx, bDis, a, aTx, aDis); // And they can only be acted upon
	}

	if (a->type == Hitbox::COMPOSITE) {
		// TODO: AABB check
		for (const Hitbox& subHit : a->composite.hitboxes) {
			if (hitboxes_overlap(&subHit, aTx, aDis, b, bTx, bDis)) return true;
		}
	}

	if (b->type == Hitbox::COMPOSITE) {
		// TODO: AABB check
		for (const Hitbox& subHit : b->composite.hitboxes) {
			if (hitboxes_overlap(a, aTx, aDis, &subHit, bTx, bDis)) return true;
		}
	}

	if (a->type == Hitbox::BOX) {
		if (b->type == Hitbox::BOX) {
			if (aTx.is_rect_invariant() && bTx.is_rect_invariant()) {
				return box_box_test(aTx * a->box, bTx * b->box);
			}
			//else {
			//	if (!box_box_test(aTx * a->box, bTx * b->box)) return false;

			//	Vector2 polyA[4], polyB[4];
			//	aabb_to_poly(a->box, polyA);
			//	aabb_to_poly(b->box, polyB);
			//	for (int i = 0; i < 4; ++i) {
			//		polyA[i] = aTx * polyA[i];
			//		polyB[i] = bTx * polyB[i];
			//	}
			//	return poly_poly_test(Array<Point2>(polyA, 4), Array<Point2>(polyB, 4));
			//}
		}
		else if (b->type == Hitbox::CIRCLE) {
			if (aTx.is_rect_invariant() && bTx.is_uniform_scale()) {
				return box_circle_test(aTx * a->box, bTx * b->circle);
			}
			else {
				// TODO approximate with polygons
			}
		}
		else if (b->type == Hitbox::LINE || b->type == Hitbox::ONEWAY) {

		}
	}
	else if (a->type == Hitbox::CIRCLE) {
		if (b->type == Hitbox::CIRCLE) {
			if (aTx.is_uniform_scale() && bTx.is_uniform_scale()) {
				return circle_circle_test(aTx * a->circle, bTx * b->circle);
			}
			else {
				// TODO approximate with polygons
			}
		}
		else if (b->type == Hitbox::LINE || b->type == Hitbox::ONEWAY) {
			if (aTx.is_uniform_scale()) {
				circle_line_test(aTx * a->circle, bTx * b->line);
			}
		}
		else if (b->type == Hitbox::BOX) {
			return hitboxes_overlap(b, bTx, bDis, a, aTx, aDis);
		}
	}
	else if (a->type == Hitbox::LINE) {
		if (b->type == Hitbox::LINE) {
			return line_line_test(aTx * a->line, bTx * b->line);
		}
		else if (b->type == Hitbox::ONEWAY) {
			Line lineA = aTx * a->line;
			Line lineB = bTx * b->line;
			if (!line_line_test(lineA, lineB)) return false;

			// the lines intersect :O
			Vector2 netDis = aDis - bDis;
			Vector2 lineVec = lineB.p2 - lineB.p1;

			// the lines are moving in the wrong direction
			if (lineVec.cross(netDis) < 0.f) return false;

			// check that the lines were not previously intersecting
			// This (sort of) handles the case where an object barely crosses it while moving the wrong direction
			// then reverses direction. (think of the launching glitch in line rider)
			return !line_line_test(
				{lineA.p1 + aDis, lineA.p2 + aDis},
				{lineB.p1 + bDis, lineB.p2 + bDis}
			);
		}
		else return hitboxes_overlap(b, bTx, bDis, a, aTx, aDis);
	}
	else if (a->type == Hitbox::POLYGON) {

	}
	printf("WARNING: Reached default case of hitboxes_overlap(...)\n");
	return false;
}

bool box_box_test(AABB a, AABB b) {
	return (
		a.left < b.right && a.right > b.left &&
		a.top < b.bottom && a.bottom > b.top
	);
}

bool box_circle_test(AABB box, Circle circ) {
	if (circ.center.x < box.left) {
		// center is left of box
		if (circ.center.y < box.top) {
			// in top-left corner
			float dx = circ.center.x - box.left;
			float dy = circ.center.y - box.top;
			return dx * dx + dy * dy < circ.radius * circ.radius;
		}
		else if (circ.center.y > box.bottom) {
			// in bottom-left corner
			float dx = circ.center.x - box.left;
			float dy = circ.center.y - box.bottom;
			return dx * dx + dy * dy < circ.radius * circ.radius;
		}
		else {
			// just to the left
			return circ.center.x < box.left - circ.radius;
		}
	}
	else if (circ.center.x > box.right) {
		// center is left of box
		if (circ.center.y < box.top) {
			// in top-right corner
			float dx = circ.center.x - box.right;
			float dy = circ.center.y - box.top;
			return dx * dx + dy * dy < circ.radius * circ.radius;
		}
		else if (circ.center.y > box.bottom) {
			// in bottom-right corner
			float dx = circ.center.x - box.right;
			float dy = circ.center.y - box.bottom;
			return dx * dx + dy * dy < circ.radius * circ.radius;
		}
		else {
			// just to the right
			return circ.center.x > box.right + circ.radius;
		}
	}
	else {
		if (circ.center.y < box.top) {
			// above
			return circ.center.y < box.top - circ.radius;
		}
		else if (circ.center.y > box.bottom) {
			// below
			return circ.center.y > box.bottom + circ.radius;
		}
		else {
			// center is inside box
			return true;
		}
	}
}

bool circle_circle_test(Circle a, Circle b) {
	float dx = a.center.x - b.center.x;
	float dy = a.center.y - b.center.y;
	float dist = a.radius + b.radius;

	return dx * dx + dy * dy < dist * dist;
}

bool circle_line_test(Circle circle, Line line) {
	Vector2 parallel = line.p2 - line.p1;
	float parlen = parallel.magnitude();
	parallel /= parlen;
	Vector2 to_circle = circle.center - line.p1;
	// project center onto line
	float projpos = to_circle.dot(parallel);
	if (projpos < 0) { // outside the line on the first point side
		// distance to endpoint
		return to_circle.x * to_circle.x + to_circle.y * to_circle.y 
			            < circle.radius * circle.radius;
	}
	else if (projpos > parlen) { // outside the line on the second point side
		Vector2 other = circle.center - line.p2;
		// distance to endpoint
		return other.x * other.x + other.y * other.y
			            < circle.radius * circle.radius;
	}
	return fabs(to_circle.cross(parallel)) < circle.radius;
}

static bool line_line_test(Line a, Line b) {
	Vector2 aVec = a.p2 - a.p1;
	Vector2 bVec = b.p2 - b.p1;
	// test intersection via rotation directions
	// i.e. if for both lines the endpoints of the other line lie on opposite sides of the line
	// probably inefficient- currently 24 floating point ops + function call overhead
	return
		signbit(aVec.cross(b.p1 - a.p1)) != signbit(aVec.cross(b.p2 - a.p1)) &&
		signbit(bVec.cross(a.p1 - b.p1)) != signbit(bVec.cross(a.p2 - b.p1));
}