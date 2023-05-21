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

## Waybar Integration

A custom waybar module can be used to display an icon when any application is
using your mic or playing any audio.

Add the following section to your `~/.config/waybar/config` file and add
`custom/audio_idle_inhibitor` to either the `modules-left`, `modules-center`
or `modules-right` list.

*Note: The FontAwesome font is used for the icons below*

```
	"custom/audio_idle_inhibitor": {
		"format": "{icon}",
		"exec": "sway-audio-idle-inhibit --dry-print-both-waybar",
		"exec-if": "which sway-audio-idle-inhibit",
		"return-type": "json",
		"format-icons": {
			"output": "",
			"input": "",
			"output-input": "  ",
			"none": ""
		}
	},
```

