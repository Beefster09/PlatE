

class Simple : EntityComponent {
    string sprite;
    Vector2 pos, vel, grav;

    Simple(string sprite, Vector2 pos, Vector2 vel, Vector2 grav) {
        this.sprite = sprite;
        this.pos = pos;
        this.vel = vel;
        this.grav = grav;
    }

    void init(Entity@ ent) {
        ent.set_sprite(sprite);
        ent.position = pos;
        ent.velocity = vel;
        ent.acceleration = grav;
    }

    void update(Entity@ ent, float delta_seconds) {}
}

void init() {
	println("Hello from init() script");

    Engine.travel("data/test.level");

    EntitySystem.spawn(Simple("data/test.sprite", Vector2(50, 400), Vector2(120, -150), Vector2(0, 100)));
    EntitySystem.spawn(Simple("data/test.sprite", Vector2(400, 300), Vector2(120, 0), Vector2(0, 0)));
}

void update(float delta_seconds) {
}
