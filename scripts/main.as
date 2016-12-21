
int init() {
	println("asdf");
	
	Vector2 a = {3, 4};
	Vector2 b = {2, 8};
	Vector2 c = {1.23, 4.56};
	
	println(a.magnitude);
	println(b);
	println((a + b).magnitude);
	println(distance(a, b));
	println(a.distance_to(b));
	println(lerp(a, b, 0.5));
	println(a.dot(b));
	println(a.cross(b));
	println(b.angle);
	println(a.rotated90CW);
	println(c.floor);
	println(a - b);
	println(Vector2_polar(PI / 6, 8));
	println(5 * a);
	println(a * 5);
	println(a / 2);
	println(Vector2_up.cross(Vector2_right));
	println(str(Vector2_zero));
	
	return 42;
}