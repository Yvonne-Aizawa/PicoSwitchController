let
  nixpkgs = fetchTarball "https://github.com/NixOS/nixpkgs/tarball/nixos-23.11";
  pkgs = import nixpkgs { config = {}; overlays = []; };
in
pkgs.mkShell {
  packages = with pkgs; [
    # Build tools
    gnumake
    cmake

    # ARM embedded toolchain for Pico (cross-compilation)
    gcc-arm-embedded

    # Python for Pico SDK scripts
    python3

    # Additional utilities
    git
  ];

  shellHook = ''
    export PICO_SDK_PATH=~/Documents/Programming/pico-sdk
    echo "Raspberry Pi Pico development environment loaded"
    echo "ARM GCC: $(arm-none-eabi-gcc --version | head -n1)"
    echo "CMake: $(cmake --version | head -n1)"
    echo "PICO_SDK_PATH: $PICO_SDK_PATH"
  '';
}