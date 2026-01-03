#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "Building kdedimmer..."
cmake -B build
cmake --build build

echo "Installing binary to ~/.local/bin..."
mkdir -p ~/.local/bin
cp build/kdedimmer ~/.local/bin/

echo "Installing systemd user service..."
mkdir -p ~/.config/systemd/user
cp kdedimmer.service ~/.config/systemd/user/

echo "Reloading systemd and enabling service..."
systemctl --user daemon-reload
systemctl --user enable --now kdedimmer

echo "Done! kdedimmer is now running."
echo ""
echo "Commands:"
echo "  kdedimmer +5       Increase opacity"
echo "  kdedimmer -5       Decrease opacity"
echo "  kdedimmer set 50   Set to 50%"
echo "  kdedimmer toggle   Toggle on/off"
echo "  kdedimmer status   Check status"
