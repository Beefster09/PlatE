# PlatE
PlatE aims to be a core executable engine designed for 2D platformers. A game designer need only provide levels, assets, and behaviors. It is meant to be _specialized_, lightweight, and modern. It is not intended to be as multi-purpose or sophisticated as Unreal, Unity, and similar engines.

Note that I'm still nailing down what I want out of this engine, so everything here is subject to change.

## What makes PlatE different from other game engines?
  + Platforming first. While pretty much any 2D game is theoretically possible, everything here is designed with platformers in mind.
  + The physics engine is designed to be _simple_ and easy to work with for platformers.
    * Unity, Unreal, Cocos2dx, and others use sophisticated rigid body physics. While that will be the right tool for some games, it frequently results in fighting with the physics engine to get things to work the way you want them to.
    * The physics system has no concept of mass, force, impulse, inertia, angular momentum, etc. (and it never will) Just set position, velocity, and acceleration directly and move on with your life.
  + Specialized: Limited, yet flexible
    * They say limitation is the mother of creativity.
	* It's better to have something that does a 95% job at 15% of tasks than to have something that does a 15% job at 95% of tasks. Most other game engines are so nebulously multi-purpose that you end up re-inventing the wheel or spend hours trying to comprehend basic things about the system.
	  - A lot of features commonly used in 2D platformers, such as slopes, an entity-component system, etc... will be built right into the engine rather than leaving their processing up to scripts.
  + Game saves will be easy to set up. There will be simple options for compression, encryption, and checksums, making them resistant to cheating.
  + Focus on sprite and tilemap based graphics.
    * Tilemaps will be able to be changed programmatically. This will include the ability to replace entire sections all at once from predefined (in the asset, that is) submaps.
    * Non-tile level objects will still be around and will be necessary and/or more convenient for some things, though hopefully they won't be needed much outside interactive level elements when dynamic tiles are not enough.
  + There will (eventually) be support for false color-map + line map + normal map shaded/paletted sprites and tiles similar to the tech used in Skullgirls and Indivisible. Palettes are a big deal for sprites and tiles since you can't just change out the textures (since they _are_ textures) like you can with meshes.
  + No plans to support netplay. This is a big-idea feature that comes with a lot of complexity that takes away from the goals of this engine. Multiplayer is still possible, but only locally. If you need netplay, use another engine.
  + No plans for 2.5D support. Use another engine for that.

## Will it be multiplatform?
All PC platforms will be natively supported by version 1.0, though I'm currently doing all my development in Visual Studio using 64-bit SDL. No guarantees it will compile everywhere at this point, however I have been avoiding using system-dependent APIs (Win32, DirectX, pthread, etc...), so the porting process should be fairly straightforward.

Console ports will probably be possible without too much effort, but they are very low priority.

## What does PlatE stand for?
**Plat**former **E**ngine

I have no idea exactly I came up with the name, but I can't think of a better one.

## When will PlatE be ready?
It's hard to say. This is my nights-and-weekends hobby project that I started at the beginning of my Senior year in university.

## How can I help?
I'm not exactly sure at this point. I'm still solidifying the goals of this engine
