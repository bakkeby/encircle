# `encircle`: A Xinerama X cursor monitor wrapper

`encircle` is a window manager agnostic tool that wraps your X cursor around the
edges of the screen and is specifically designed to work with asymmetric
multi-monitor Xinerama setups.

In practice what this means is that the screen area is turned into a flat
torus in that when the cursor hits a screen edge then it will continue and
reappear on the opposite edge.

If the cursor position is not within the target screen then the cursor will
snap to the nearest edge of that screen.

By default cursor wrapping is enabled on both horizontal and vertical axes.

```c
   ┌───────┐
   │     1┄┤
   │       │┌───────────────^─┐┌───────┐
   │       │├┄1┄>           4 ││       │
   │       ││                 ││       │
   │   A   ││        B        ││   C   │
   │       ││                 ││       │
   ├┄2     │├┄3┄>             ││   <┄2┄┤
   │       │└───────────────^─┘│       │
   │     3┄┤                   │       │
   └───────┘                   └───────┘
```

In the above example we have four transitions:
   1) snaps to the adjacent monitor (rather than hitting a hard wall)
   2) wraps the cursor from monitor A to monitor C which is the rightmost monitor
   3) snaps to the adjacent monitor (rather than hitting a hard wall)
   4) wraps around monitor B because there is no other monitor above or below

If the monitor arrangement is changed (e.g. using xrandr) then encircle will be
notified of that and update monitor positions and dimensions accordingly.

## Installation

```
$ git clone https://github.com/bakkeby/encircle.git
$ cd encircle
$ sudo make install
$ encircle
```

This is also available in the [AUR](https://aur.archlinux.org/packages/encircle).

## Dependencies

### Arch

- base-devel
- extra/libxinerama
- libxi
- libxfixes

```sh
sudo pacman -S base-devel extra/libxinerama libxi libxfixes
```

### Debian

- build-essential
- libxinerama-dev
- libxi-dev
- libxfixes-dev

```sh
sudo apt install build-essential libxinerama-dev libxi-dev libxfixes-dev
```

### Void

- base-devel
- libXinerama-devel
- libXi-devel
- libXfixes-devel

```sh
xbps-install -Su base-devel libXinerama-devel libXi-devel libXfixes-devel
```

## Credits

This work is largely derived from the [dwm](https://dwm.suckless.org/) window manager in relation
to its handling of monitors in a Xinerama setup. The work is inspired by
[xoop](https://mcol.xyz/code/xoop/) and [taralli](https://github.com/kmcallister/taralli) and
+[xbanish](https://github.com/jcs/xbanish).

## License

`encircle` is available under an MIT license. See the `LICENSE` file.
