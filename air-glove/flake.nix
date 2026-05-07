{
  description = "Air Glove — ESP32 + MPU6050 + capacitive touch + BLE HID — dev shell";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        py = pkgs.python3.withPackages (ps: with ps; [ pyserial numpy evdev ]);

        # PlatformIO downloads prebuilt xtensa-esp32-elf toolchains that are
        # linked against generic libc / ld-linux. Those won't run on NixOS
        # outside an FHS environment, so wrap the whole shell with
        # buildFHSEnv to expose /lib64/ld-linux-x86-64.so.2 etc.
        fhs = pkgs.buildFHSEnv {
          name = "air-glove-dev";
          targetPkgs = p: (with p; [
            platformio-core
            esptool
            picocom
            py
            gnumake
            git
            udev
            libusb1
            ncurses
            zlib
            stdenv.cc.cc.lib
          ]);
          # Args after `nix run .` are passed here as "$@".
          runScript = pkgs.writeShellScript "air-glove-dev-entry" ''
            export PLATFORMIO_CORE_DIR="$PWD/.pio-core"
            mkdir -p "$PLATFORMIO_CORE_DIR"
            export PORT="''${PORT:-/dev/ttyUSB0}"
            export PIO_ENV="''${PIO_ENV:-esp32dev}"
            export BAUD="''${BAUD:-115200}"
            if [ "$#" -gt 0 ]; then
              exec "$@"
            else
              echo "air-glove dev shell (ESP32 / PlatformIO, FHS env)"
              echo "  PORT=$PORT  PIO_ENV=$PIO_ENV  BAUD=$BAUD"
              echo "  build:    pio run -e $PIO_ENV"
              echo "  upload:   pio run -e $PIO_ENV -t upload --upload-port $PORT"
              echo "  monitor:  pio device monitor -p $PORT -b $BAUD"
              echo "  test:     pio test -e native"
              exec bash
            fi
          '';
        };
      in {
        # `nix develop`  → enter FHS shell.
        # `nix run . -- pio run -e esp32dev -t upload`  → run inside FHS.
        devShells.default = pkgs.mkShell {
          packages = [ fhs ];
          shellHook = ''
            if [ -z "$AIR_GLOVE_FHS" ]; then
              export AIR_GLOVE_FHS=1
              exec air-glove-dev
            fi
          '';
        };
        apps.default = {
          type = "app";
          program = "${fhs}/bin/air-glove-dev";
        };
        packages.default = fhs;
      });
}
