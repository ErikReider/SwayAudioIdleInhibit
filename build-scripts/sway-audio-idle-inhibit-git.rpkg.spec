# vim: syntax=spec
Name:       sway-audio-idle-inhibit-git
Version:    {{{ git_repo_release lead="$(git describe --tags --abbrev=0)" }}}
Release:    {{{ echo -n "$(git rev-list --all --count)" }}}%{?dist}
Summary:    Prevents swayidle from sleeping while any application is outputting or receiving audio.
License:    GPLv3
URL:        https://github.com/ErikReider/SwayAudioIdleInhibit
VCS:        {{{ git_repo_vcs }}}
Source:     {{{ git_repo_pack }}}

BuildRequires: meson >= 0.60.0
BuildRequires: git
BuildRequires: gcc-c++

BuildRequires: pkgconfig(libpulse)
BuildRequires: pkgconfig(libsystemd)

%{?systemd_requires}

%description
Prevents swayidle from sleeping while any application is outputting or receiving audio.
Should work with all Wayland desktops that support the zwp_idle_inhibit_manager_v1 protocol but only tested in Sway

This only works for Pulseaudio / Pipewire Pulse

%prep
{{{ git_repo_setup_macro }}}

%build
%meson
%meson_build

%install
%meson_install

%files
%doc README.md
%{_bindir}/sway-audio-idle-inhibit
%license LICENSE

# Changelog will be empty until you make first annotated Git tag.
%changelog
{{{ git_repo_changelog }}}
