# SwayAudioIdleInhibit

Prevents swayidle from sleeping while any application is outputting or
receiving audio. Should work with all Wayland desktops that support the
`zwp_idle_inhibit_manager_v1` protocol but only tested in Sway

This only works for Pulseaudio / Pipewire Pulse

## Install

Arch:
The package is available on the [AUR](https://aur.archlinux.org/packages/sway-audio-idle-inhibit-git/)

Other:

```zsh
meson build
ninja -C build
meson install -C build
```

## Sway Usage

```ini
# Enables inhibit_idle when playing audio
exec sway-audio-idle-inhibit
```

## Other usages without inhibiting idle

These could be used to monitor if any application is using your mic or playing
any audio.

Monitor sources and sinks: will print `RUNNING` or `NOT RUNNING`

```zsh
sway-audio-idle-inhibit --dry-print-both
```

Monitor sources: will print `RUNNING` or `NOT RUNNING`

```zsh
sway-audio-idle-inhibit --dry-print-source
```

Monitor sinks: will print `RUNNING` or `NOT RUNNING`

```zsh
sway-audio-idle-inhibit --dry-print-sink
```
