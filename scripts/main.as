
bool first = true;
class Simple : EntityComponent {
    string sprite;
    Vector2 pos, vel, grav;
    TestController@ cont;

    Simple(string sprite, Vector2 pos, Vector2 vel, Vector2 grav) {
        this.sprite = sprite;
        this.pos = pos;
        this.vel = vel;
        this.grav = grav;

        if (first) {
            @cont = get_TestController_instance(0);
            first = false;
        }
    }

    void init(Entity@ ent) {
        ent.set_sprite(sprite, 2);
        ent.position = pos;
        ent.velocity = vel;
        ent.acceleration = grav;

        if (cont !is null) {
            cont.bind(ent);
        }
    }

    void update(Entity@ ent, float delta_seconds) {
    }

    void on_press_Up(Entity@ ent) {
        println("+Up");
    }

    void on_release_Up(Entity@ ent) {
        println("-Up");
    }

    void on_press_Down(Entity@ ent) {
        println("+Down");
    }

    void on_release_Down(Entity@ ent) {
        println("-Down");
    }

    void on_press_OK(Entity@ ent) {
        println("+OK");
    }

    void on_release_OK(Entity@ ent) {
        println("-OK");
    }
}

void PrintErr(int code, const string &in desc) {
    println("Error #" + code + ": " + desc);
}

void init() {
	println("Hello from init() script");

    Engine.travel("data/test.level");

    EntitySystem.spawn(Simple("data/test.sprite", Vector2(50, 400), Vector2(120, -350), Vector2(0, 500)), PrintErr);
    EntitySystem.spawn(Simple("data/test.sprite", Vector2(400, 300), Vector2(10, 0), Vector2(0, 0)), PrintErr);

    Engine.pause();
}

void update(float delta_seconds) {
    if (Engine.time > 1.5f) {
        Engine.resume();
    }
}
