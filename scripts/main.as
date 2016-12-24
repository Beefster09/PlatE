
void init() {
	println("Hello from init() script");
}

void update(float delta_seconds) {
    //println(Engine_time_seconds());
    if (Engine_time_seconds() > 1.0) {
        Engine_pause();
    }
}
