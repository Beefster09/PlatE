# PlatE
PlatE aims to be a core executable engine designed for 2D platformers. A game designer need only provide levels, assets, and behaviors. It is meant to be _specialized_ and fast.

Note that I'm still nailing down what I want out of this engine, so everything here is subject to change.

## What makes PlatE different from other game engines?
Of course. The selling point. Other than the obvious of being open-source MIT-licensed:

  + The physics engine is designed to be _simple_ and give a very platformy feel.
    * Well except for the hitbox system, which is inspired by fighting games.
    * Unity, Unreal, Cocos2dx, and others use more sophisticated rigid body physics. While that will be the right tool for some games, it can often be unwieldy overkill when it comes to 2D platformers. Not to mention that those engines struggle with some of the simple things we expect out of platformers, such as slopes. So often with those engines, you end up implementing your own physics to get the feel that is right for your game.
  + The entity system is data-oriented (think particle systems), making it fast and parallelizable.
    * This helps avoid using certain tricks commonly used in other engines such as object pooling, as it is essentially an object pool in itself.
	* Memory allocation in general is minimized, leading to far less memory fragmentation.
  + Specialized: Limited, yet flexible
    * They say limitation is the mother of creativity.
	* It's better to have something that does a 95% job at 15% of tasks than to have something that does a 15% job at 95% of tasks. Most other game engines are so nebulously multi-purpose that you end up re-inventing the wheel or spend hours trying to comprehend basic things about the system.
	* There _will_ be a scripting system. At this point, it's unclear to me whether this will be something like Lua or a simpler VM-based homebrew scripting language. I want the scripting engine to be parallelizable somehow, sandboxed, and fast.
  + It will be easy to implement game saves. (Hopefully as mindlessly as Zelda Classic)
  
## Influences and inspiration
Probably my biggest (most recent) inspiration was watching the Indivisible development streams with Mike Z. Seeing the hitbox system and... fighting game physics... applied to a 2D platformer made me realize that that was probably the best way to go about collision detection - lots of smaller hitboxes. Except instead of taking the engine from fighter to platformer, I'd start from scratch from the platformer angle.

Sonic and Freedom Planet acted as inspiration. I want to make it possible at some point to have loop-de-loop physics and such _just work_ in the engine.

Lastly, Zelda Classic and RPG Maker are sort of my yardsticks when it comes to the engine and editor usability (which will come later and will probably be written in a different language since performance is not so critical there). Making a playable 1st iteration prototype of a game should be simple and fast if you have the assets.
	
## Will it be multiplatform?
At some point, yes, though I'm currently doing all my development in Visual Studio using 64-bit SDL. No guarantees it will compile everywhere at this point.

I'm a big fan of Linux, so you guys will definitely be 100% supported by version 1.0.

And yes, Mac OS will get support too... Though I will need a community to be able to help with that.

Console ports will probably be possible without too much effort, but they are very low priority.
	
## What does PlatE stand for?
**Plat**former **E**ngine

I have no idea exactly I came up with the name, but I can't think of a better one.
  
## When will PlatE be ready?
It's hard to say. This is my nights-and-weekends hobby project that I started at the beginning of my Senior year in university.

## How can I help?
I'm not exactly sure at this point. I'm still solidifying the goals of this engine