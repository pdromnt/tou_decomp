# Decompilation of Tunnels of Underworld

DISCLAIMER: This is project is heavily incomplete and doesn't function at all yet. The original source code I'm assuming was lost, so I'm attempting an effort to bring this game back to life, first by decompiling it and then by updating it so it works properly in current day Windows versions.

Tunnels of Underworld (or TOU for short) is an original creation by [hannukp](https://github.com/hannukp) and I don't claim any ownership over it. I'm simply a fan who wants to preserve his game.

## What works so far

Almost nothing. The code I got is essentialy heavily decompiling things with Ghidra and having Gemini help me with the hardest parts. Consider it like the scaffolding of what should be the game, I didn't even get the menus showing yet.

- Music
- Basic sprite/image rendering
- Some basic game states

## Objective

Hopefully pull off a 95% compatible decomp, where we can fire up the game and play through the levels.  
I also plan on making a level viewer in the style of my other project, [Hotzone](https://github.com/pdromnt/uprising-level-editor), to hopefully better understand how the levels (and maybe GG Packs) work.  

Later on, when we have a working game, I plan to update the stack to add more compatibility and maybe some improvements.

## Contribs

If you know enough to help with the decomp, feel free to open a PR.

## Tools used
- MinGW
- Ghidra
- Antigravity + BetterGhidraMCP
- An old Pentium II running Windows 98 to run the original game.

## License

I'm not gonna put any kind of licenses on this project due to it not being a clean room decomp and also in respect to the original creator. I'm not sure if he would be mad at me if I even did this decomp! (Sorry Hannu!)
