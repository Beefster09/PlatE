
#include "sprite.h"
#include "hitbox.h"
#include "transform.h"
#include "fileutil.h"
#include <SDL_gpu.h>
#include <cstring>
#include <cassert>

Hitbox::Hitbox(const Hitbox& other) {
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
	case NONE:
	default:
		return;
	}
}

void Hitbox::operator = (const Hitbox& other) {
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
	case NONE:
	default:
		return;
	}
}

Array2D<bool> ColliderType::table = Array2D<bool>();
Array<const ColliderType> ColliderType::types = Array<const ColliderType>();

const ColliderType* ColliderType::by_name(const char* name) {
	for (const ColliderType& cgroup : ColliderType::types) {
		if (strcmp(name, cgroup.name) == 0) return &cgroup;
	}
	printf("Warning: '%s' is not a valid ColliderGroup.\n", name);
	return nullptr;
}

constexpr std::initializer_list<ColliderType> BUILTIN_COLLIDER_TYPES = {};
constexpr int N_BUILTIN_COLLIDER_TYPES = BUILTIN_COLLIDER_TYPES.size();

Result<> ColliderType::init(FILE* bootloader) {
	try {
		int n_types = read<uint16_t>(bootloader) + N_BUILTIN_COLLIDER_TYPES;

		ColliderType* types = new ColliderType[n_types];
		ColliderType::table = Array2D<bool>(n_types, n_types);
		table.clear();

		int i = 0;

		// init intrinsics
		for (const ColliderType& intrinsic : BUILTIN_COLLIDER_TYPES) {
			types[i] = intrinsic;
			types[i].id = i;
			++i;
		}

		for (; i < n_types; ++i) {
			types[i].name = read_string(bootloader, read<uint16_t>(bootloader));
			types[i].id = i;
			uint8_t
				r = read<uint8_t>(bootloader),
				g = read<uint8_t>(bootloader),
				b = read<uint8_t>(bootloader);
			types[i].color = { r, g, b, 255 };

			int n_rel = read<uint16_t>(bootloader);
			for (int rel = 0; rel < n_rel; ++rel) {
				table.set(i, read<uint16_t>(bootloader));
			}
		}

		ColliderType::types = Array<const ColliderType>(types, n_types);

		return Result<>::success;
	}
	catch (Error& e) {
		return e;
	}
}

Array<const ColliderChannel> ColliderChannel::channels;

constexpr const char* BUILTIN_COLLIDER_CHANNELS[] = {
	"EntityDefault",
	"TilemapDefault"
};
constexpr int N_BUILTIN_COLLIDER_CHANNELS = sizeof(BUILTIN_COLLIDER_CHANNELS) / sizeof(const char*);

Result<> ColliderChannel::init(FILE* stream) {
	try {
		int n_chans = read<uint16_t>(stream) + N_BUILTIN_COLLIDER_CHANNELS;

		if (n_chans > 64) return Errors::TooManyColliderChannels;

		ColliderChannel* chans = new ColliderChannel[n_chans];

		int i = 0;
		for (; i < N_BUILTIN_COLLIDER_CHANNELS; ++i) {
			chans[i].name = BUILTIN_COLLIDER_CHANNELS[i];
			chans[i].id = i;
		}

		for (; i < n_chans; ++i) {
			chans[i].name = read_string(stream, read<uint16_t>(stream));
			chans[i].id = i;
		}
		new(&channels) Array<const ColliderChannel>(chans, n_chans);

		return Result<>::success;
	}
	catch (Error& e) {
		return e;
	}
}

#pragma region ScriptChannelMaskOps
// Scripting interface to collider things
static void AssignChannelID(uint64_t* chan, uint8_t id) {
	*chan = BIT64(id);
}

static void AddChannelID(uint64_t* chan, uint8_t id) {
	*chan |= BIT64(id);
}

static void RemoveChannelID(uint64_t* chan, uint8_t id) {
	*chan &= ~BIT64(id);
}

static uint64_t WithChannelID(const uint64_t* chan, uint8_t id) {
	return *chan | BIT64(id);
}

static uint64_t WithoutChannelID(const uint64_t* chan, uint8_t id) {
	return *chan & ~BIT64(id);
}

static bool MatchChannelID(const uint64_t* chan, uint8_t id) {
	return *chan & BIT64(id);
}

static uint64_t CombineChannelID(const uint8_t* a, uint8_t b) {
	return BIT64(*a) | BIT64(b);
}

static uint64_t UnionChannelMask(const uint64_t* chan, uint64_t other) {
	return *chan | other;
}

static uint64_t IntersectChannelMask(const uint64_t* chan, uint64_t other) {
	return *chan & other;
}

static uint64_t SymDiffChannelMask(const uint64_t* chan, uint64_t other) {
	return *chan & other;
}

static uint64_t DiffChannelMask(const uint64_t* chan, uint64_t other) {
	return *chan & ~other;
}

static void UnionChannelMaskAssign(uint64_t* chan, uint64_t other) {
	*chan |= other;
}

static void IntersectChannelMaskAssign(uint64_t* chan, uint64_t other) {
	*chan &= other;
}

static void SymDiffChannelMaskAssign(uint64_t* chan, uint64_t other) {
	*chan &= other;
}

static void DiffChannelMaskAssign(uint64_t* chan, uint64_t other) {
	*chan &= ~other;
}

static void PrintChannelMask(uint64_t chan) {
	char buf[65];
	for (int i = 0; i < ColliderChannel::channels.size(); ++i) {
		buf[i] = (chan & BIT64(i)) ? '1' : '0';
	}
	buf[ColliderChannel::channels.size()] = 0;
	printf("%s\n", buf);
}

static void PrintChannelID(uint8_t id) {
	printf("%s\n", ColliderChannel::channels[id].name);
}
#pragma endregion

static uint64_t AllChannels = 0xFFFFffffFFFFffff;
static uint64_t NoChannels = 0xFFFFffffFFFFffff;

#define check(EXPR) {int r = (EXPR); if (r < 0) return r;} do {} while(0)
int RegisterColliderTypes(asIScriptEngine* engine) {
	check(engine->RegisterObjectType("ChannelMask", sizeof(uint64_t),
		asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<uint64_t>()));

	// I would do an enum, but I need a specific size.
	// The bonus is that it gives me very tight control over operators
	check(engine->RegisterObjectType("ChannelID", sizeof(uint8_t),
		asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<uint8_t>()));

	check(engine->SetDefaultNamespace("ChannelID"));
	for (const ColliderChannel& chan : ColliderChannel::channels) {
		char declbuf[256];
		sprintf(declbuf, "const ChannelID %s", chan.name);
		check(engine->RegisterGlobalProperty(declbuf, (void*) &chan.id));
	}

	check(engine->SetDefaultNamespace("ChannelMask"));
	check(engine->RegisterGlobalProperty("const ChannelMask ALL", (void*)&AllChannels));
	check(engine->RegisterGlobalProperty("const ChannelMask NONE", (void*)&NoChannels));

	check(engine->SetDefaultNamespace(""));

	// TODO: adding/removing ids to/from the mask, combining masks
	check(engine->RegisterObjectMethod("ChannelMask", "void opAssign(ChannelID)",
		asFUNCTION(AssignChannelID), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "void opAddAssign(ChannelID)",
		asFUNCTION(AddChannelID), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "void opOrAssign(ChannelID)",
		asFUNCTION(AddChannelID), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "void opSubAssign(ChannelID)",
		asFUNCTION(RemoveChannelID), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod("ChannelMask", "void opOrAssign(ChannelMask)",
		asFUNCTION(UnionChannelMaskAssign), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "void opAddAssign(ChannelMask)",
		asFUNCTION(UnionChannelMaskAssign), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "void opAndAssign(ChannelMask)",
		asFUNCTION(IntersectChannelMaskAssign), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "void opXorAssign(ChannelMask)",
		asFUNCTION(SymDiffChannelMaskAssign), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "void opSubAssign(ChannelMask)",
		asFUNCTION(DiffChannelMaskAssign), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod("ChannelMask", "ChannelMask opAdd(ChannelID) const",
		asFUNCTION(WithChannelID), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "ChannelMask opOr(ChannelID) const",
		asFUNCTION(WithChannelID), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "ChannelMask opSub(ChannelID) const",
		asFUNCTION(WithoutChannelID), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod("ChannelID", "ChannelMask opAdd(ChannelID) const",
		asFUNCTION(CombineChannelID), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelID", "ChannelMask opOr(ChannelID) const",
		asFUNCTION(CombineChannelID), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod("ChannelMask", "bool contains(ChannelID) const",
		asFUNCTION(MatchChannelID), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "bool opAnd(ChannelID) const",
		asFUNCTION(MatchChannelID), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterObjectMethod("ChannelMask", "ChannelMask opOr(ChannelMask) const",
		asFUNCTION(UnionChannelMask), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "ChannelMask opAdd(ChannelMask) const",
		asFUNCTION(UnionChannelMask), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "ChannelMask opAnd(ChannelMask) const",
		asFUNCTION(IntersectChannelMask), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "ChannelMask opXor(ChannelMask) const",
		asFUNCTION(SymDiffChannelMask), asCALL_CDECL_OBJFIRST));
	check(engine->RegisterObjectMethod("ChannelMask", "ChannelMask opSub(ChannelMask) const",
		asFUNCTION(DiffChannelMask), asCALL_CDECL_OBJFIRST));

	check(engine->RegisterGlobalFunction("void println(const ChannelMask)",
		asFUNCTION(PrintChannelMask), asCALL_CDECL));
	check(engine->RegisterGlobalFunction("void println(const ChannelID)",
		asFUNCTION(PrintChannelID), asCALL_CDECL));

	return 0;
}

// Core logic

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

				GPU_Line(context, tagp1.x, tagp1.y, tagp1.x + tag.x, tagp1.y + tag.y, fill);
			}
		}
	}
		break;
	case Hitbox::POLYGON:
	{
		if (hitbox.polygon.vertices.size() > 32) break;
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


void render_colliders(GPU_Target* context, const Transform& tx, const Array<const Collider>& colliders) {
	for (const Collider& collider: colliders) {
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
	const Hitbox& a, const Transform& aTx, Vector2 aDis, // aDis and bDis are the displacement since last frame
	const Hitbox& b, const Transform& bTx, Vector2 bDis  // They are only needed for checking oneway collisions
) {
	if (a.type == Hitbox::NONE || b.type == Hitbox::NONE) return false;

	if (a.type == Hitbox::ONEWAY) {
		if (b.type == Hitbox::ONEWAY) return false; // One-ways don't collide with each other
		else return hitboxes_overlap(b, bTx, bDis, a, aTx, aDis); // And they can only be acted upon
	}

	if (a.type == Hitbox::COMPOSITE) {
		// TODO: AABB check
		for (const Hitbox& subHit : a.composite.hitboxes) {
			if (hitboxes_overlap(subHit, aTx, aDis, b, bTx, bDis)) return true;
		}
		return false;
	}

	if (b.type == Hitbox::COMPOSITE) {
		// TODO: AABB check
		for (const Hitbox& subHit : b.composite.hitboxes) {
			if (hitboxes_overlap(a, aTx, aDis, subHit, bTx, bDis)) return true;
		}
		return false;
	}

	if (a.type == Hitbox::BOX) {
		if (b.type == Hitbox::BOX) {
			if (aTx.is_rect_invariant() && bTx.is_rect_invariant()) {
				return box_box_test(aTx * a.box, bTx * b.box);
			}
			else {
				if (!box_box_test(aTx * a.box, bTx * b.box)) return false;

				Vector2 polyA[4], polyB[4];
				aabb_to_poly(a.box, polyA);
				aabb_to_poly(b.box, polyB);
				for (int i = 0; i < 4; ++i) {
					polyA[i] = aTx * polyA[i];
					polyB[i] = bTx * polyB[i];
				}
				return poly_poly_test(Array<Point2>(polyA, 4), Array<Point2>(polyB, 4));
			}
		}
		else if (b.type == Hitbox::CIRCLE) {
			if (aTx.is_rect_invariant() && bTx.is_uniform_scale()) {
				return box_circle_test(aTx * a.box, bTx * b.circle);
			}
			else {
				// TODO approximate with polygons
			}
		}
		else if (b.type == Hitbox::LINE || b.type == Hitbox::ONEWAY) {
			// TODO
		}
	}
	else if (a.type == Hitbox::CIRCLE) {
		if (b.type == Hitbox::CIRCLE) {
			if (aTx.is_uniform_scale() && bTx.is_uniform_scale()) {
				return circle_circle_test(aTx * a.circle, bTx * b.circle);
			}
			else {
				// TODO approximate with polygons
			}
		}
		else if (b.type == Hitbox::LINE || b.type == Hitbox::ONEWAY) {
			if (aTx.is_uniform_scale()) {
				circle_line_test(aTx * a.circle, bTx * b.line);
			}
			else {
				// TODO
			}
		}
		else if (b.type == Hitbox::BOX) {
			return hitboxes_overlap(b, bTx, bDis, a, aTx, aDis);
		}
	}
	else if (a.type == Hitbox::LINE) {
		if (b.type == Hitbox::LINE) {
			return line_line_test(aTx * a.line, bTx * b.line);
		}
		else if (b.type == Hitbox::ONEWAY) {
			Line lineA = aTx * a.line;
			Line lineB = bTx * b.line;
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
	else if (a.type == Hitbox::POLYGON) {

	}
	//assert(false && "WARNING: Reached default case of hitboxes_overlap(...)\n");
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
	// TODO: Optimize

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
	// TODO: Unwind and simplify (optimize)
	return
		signbit(aVec.cross(b.p1 - a.p1)) != signbit(aVec.cross(b.p2 - a.p1)) &&
		signbit(bVec.cross(a.p1 - b.p1)) != signbit(bVec.cross(a.p2 - b.p1));
}

static bool poly_poly_test(Array<Point2> a, Array<Point2> b) {
	return false;
}