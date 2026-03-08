# Decompilation of Tunnels of Underworld

DISCLAIMER: This is project is incomplete. The original source code I'm assuming was lost, so I'm attempting an effort to bring this game back to life, first by decompiling it and then by updating it so it works properly in current day Windows versions.

Tunnels of Underworld (or TOU for short) is an original creation by [hannukp](https://github.com/hannukp) and I don't claim any ownership over it. I'm simply a fan who wants to preserve his game.

## What works so far

A lot of things. A lot of things are also very buggy. The code I got is essentialy heavily decompiling things with Ghidra and having a clanker help me with the hardest parts.

- Renderer (Sprites, Particles, Animations, HUD, etc.)
- Menus (90% functional)
- SFX/BGM
- Controls
- Physics (Ships, water, collisions, etc.)
- Levels (including GG Levels)
- Most subsystems (Enemy AI (partial), spawns, pickups, etc.)

I'd guesstimate we're 85% of the way there.

## Objective

Initially perform a "dirty" decomp, with no organized file structure or namings, just decompiling, making sure things work, implementing systems and fixing bugs.

Later on, organize file structure, namings and overall code architecture for easy maintenance.

All in a all, the plan is to hopefully pull off a 99% compatible decomp, where we can fire up the game and play through the levels without major issues.  

I also plan on making a level viewer in the style of my other project, [Hotzone](https://github.com/pdromnt/uprising-level-editor), to hopefully better understand how the levels (and maybe GG Packs) work.  

When we're done with everything and have a fully working game, I plan to update the stack (SDL?) to add more compatibility (and maybe support for other OSs?) and maybe some improvements (Netplay, Gamepad support, experiment with Upscaling).

## Contribs

If you know enough to help with the decomp, feel free to open a PR.

## Tools used
- MinGW
- Ghidra
- Claude + BetterGhidraMCP
- An old Pentium II running Windows 98 to run the original game.

## License

I'm not gonna put any kind of licenses on this project due to it not being a clean room decomp and also in respect to the original creator. I'm not sure if he would be mad at me if I did this decomp! (Sorry Hannu!)
