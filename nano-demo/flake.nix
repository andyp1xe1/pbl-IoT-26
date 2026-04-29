{
  description = "Arduino Nano + MPU6050 + HC-05 BT — dev shell";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        py = pkgs.python3.withPackages (ps: with ps; [ pyserial numpy evdev ]);
      in {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            arduino-cli
            avrdude
            picocom
            py
            bluez            # bluetoothctl, rfcomm, hcitool
            gnumake
          ];

          # Keep arduino-cli state inside the project so the shell is self-contained.
          shellHook = ''
            export ARDUINO_DIRECTORIES_DATA="$PWD/.arduino/data"
            export ARDUINO_DIRECTORIES_DOWNLOADS="$PWD/.arduino/dl"
            export ARDUINO_DIRECTORIES_USER="$PWD/.arduino/user"
            mkdir -p "$ARDUINO_DIRECTORIES_DATA" "$ARDUINO_DIRECTORIES_DOWNLOADS" "$ARDUINO_DIRECTORIES_USER"

            export PORT="''${PORT:-/dev/ttyUSB0}"
            # Old-bootloader Nano clones (most CH340 ones) need atmega328old.
            export FQBN="''${FQBN:-arduino:avr:nano:cpu=atmega328old}"
            export BAUD="''${BAUD:-115200}"

            echo "glove dev shell"
            echo "  PORT=$PORT  FQBN=$FQBN  BAUD=$BAUD"
            echo "  first time: make setup    (installs avr core + MPU6050 lib)"
            echo "  build/upload/monitor/log: see Makefile"
          '';
        };
      });
}
