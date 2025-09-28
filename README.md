
<div align="right">
  <details>
    <summary >🌐 Language</summary>
    <div>
      <div align="center">
        <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=en">English</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=zh-CN">简体中文</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=zh-TW">繁體中文</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=ja">日本語</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=ko">한국어</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=hi">हिन्दी</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=th">ไทย</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=fr">Français</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=de">Deutsch</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=es">Español</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=it">Italiano</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=ru">Русский</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=pt">Português</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=nl">Nederlands</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=pl">Polski</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=ar">العربية</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=fa">فارسی</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=tr">Türkçe</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=vi">Tiếng Việt</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=id">Bahasa Indonesia</a>
        | <a href="https://openaitx.github.io/view.html?user=ErikReider&project=SwayAudioIdleInhibit&lang=as">অসমীয়া</
      </div>
    </div>
  </details>
</div>

# SwayAudioIdleInhibit

Prevents swayidle/hypridle from sleeping while any application is outputting or
receiving audio. Requires systemd/elogind inhibit support.

This only works for Pulseaudio / Pipewire Pulse

## Install

Arch:
The package is available on the [AUR](https://aur.archlinux.org/packages/sway-audio-idle-inhibit-git/)

Other:

```zsh
# Can compile to use systemd or elogind
# systemd (default)
meson setup build -Dlogind-provider=systemd
# or elogind for systemd-less systems
meson setup build -Dlogind-provider=elogind

meson compile -C build
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

```json
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
