
void init() {
	println("Hello from init() script");

    Engine.travel("data/test.level");
}

void update(float delta_seconds) {
    if (Engine.time > 5.f) {
        Engine.resume();
    }
    else if (Engine.time > 2.f) {
        Engine.pause();
    }
}
