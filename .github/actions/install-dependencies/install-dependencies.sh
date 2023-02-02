#!/usr/bin/env bash

# --- Dependency configuration ---
#
# ubuntu-20.04:      Qt 5 + GTK2
# ubuntu-22.04:      Qt 5 + GTK3
# Windows:           Qt 5 + GTK2
# macOS (Autotools): Qt 5 - GTK
# macOS (Meson):     Qt 6 - GTK

os=$(tr '[:upper:]' '[:lower:]' <<< "$1")
build_system=$(tr '[:upper:]' '[:lower:]' <<< "$2")

if [ -z "$os" ] || [ -z "$build_system" ]; then
  echo 'Invalid or missing input parameters'
  exit 1
fi

ubuntu_packages='gettext libadplug-dev libasound2-dev libavformat-dev
                 libbinio-dev libbs2b-dev libcddb2-dev libcdio-cdda-dev
                 libcue-dev libcurl4-gnutls-dev libdbus-glib-1-dev
                 libfaad-dev libflac-dev libfluidsynth-dev libgl1-mesa-dev
                 libjack-jackd2-dev liblircclient-dev libmms-dev libmodplug-dev
                 libmp3lame-dev libmpg123-dev libneon27-gnutls-dev libnotify-dev
                 libopenmpt-dev libopusfile-dev libpulse-dev libqt5opengl5-dev
                 libqt5x11extras5-dev libsamplerate0-dev libsdl2-dev
                 libsidplayfp-dev libsndfile1-dev libsndio-dev libsoxr-dev
                 libvorbis-dev libwavpack-dev libxml2-dev qtbase5-dev
                 qtmultimedia5-dev'

macos_packages='adplug faad2 ffmpeg libbs2b libcue libmms libmodplug libnotify
                libopenmpt libsamplerate libsoxr neon sdl2 wavpack'

case "$os" in
  ubuntu-20.04)
    if [ "$build_system" = 'meson' ]; then
      sudo apt-get -qq update && sudo apt-get install $ubuntu_packages libgtk2.0-dev meson
    else
      sudo apt-get -qq update && sudo apt-get install $ubuntu_packages libgtk2.0-dev
    fi
    ;;

  ubuntu*)
    if [ "$build_system" = 'meson' ]; then
      sudo apt-get -qq update && sudo apt-get install $ubuntu_packages libgtk-3-dev libpipewire-0.3-dev meson
    else
      sudo apt-get -qq update && sudo apt-get install $ubuntu_packages libgtk-3-dev libpipewire-0.3-dev
    fi
    ;;

  macos*)
    if [ "$build_system" = 'meson' ]; then
      brew install $macos_packages qt@6 meson
    else
      brew install $macos_packages qt@5 automake
    fi
    ;;

  windows*)
    # - Nothing to do here -
    # Packages are installed with the MSYS2 setup in the action.yml file
    # and by making use of 'paccache'.
    ;;

  *)
    echo "Unsupported OS: $os"
    exit 1
    ;;
esac
