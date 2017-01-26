
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
            cont.bind(this);

            cont.Up.bind_on_press(ButtonEventCallback(pUp));
            cont.Up.bind_on_release(ButtonEventCallback(rUp));
        }
    }

    void update(Entity@ ent, float delta_seconds) {
    }

    void pUp() {
        println("+Up");
    }

    void rUp() {
        println("-Up");
    }

    void on_press_Down() {
        println("+Down");
    }

    void on_release_Down() {
        println("-Down");
    }
}

void plusOK() {
    println("+OK");
}

void minusOK() {
    println("-OK");
}

void PrintErr(int code, const string &in desc) {
    println("Error #" + code + ": " + desc);
}

void init() {
    Engine::travel("assets/test.level");

    EntitySystem.spawn(Simple("assets/test.sprite", Vector2(50, 400), Vector2(120, -350), Vector2(0, 500)), PrintErr);
    EntitySystem.spawn(Simple("assets/test.sprite", Vector2(400, 300), Vector2(10, 0), Vector2(0, 0)), PrintErr);

    TestController@ cont = get_TestController_instance(0);

    cont.OK.bind_on_press(plusOK);
    cont.OK.bind_on_release(minusOK);

    Engine::pause();
}

void update(float delta_seconds) {
    if (Engine::time > 1.5f) {
        Engine::resume();
    }
}
