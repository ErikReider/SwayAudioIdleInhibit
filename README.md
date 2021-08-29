# SwayAudioIdleInhibit

Prevents swayidle from sleeping while any application is outputting or
receiving audio

This only works for pulseaudio / pipewire-pulse

## Install

<!-- Arch: -->
<!-- The package is available on the [AUR]() -->

Other:

```zsh
meson build
ninja -C build
meson install -C build
```

## Run

```zsh
./build/sway-audio-idle-inhibit
```
