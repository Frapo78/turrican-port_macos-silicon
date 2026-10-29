{
  description = "Turrican II (Factor 5 / Rainbow Arts) – C++/SDL2 port of the Amiga version";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        version = "0.1.0";
        turrican2 = pkgs.stdenv.mkDerivation {
          pname = "turrican2";
          inherit version;
          src = self;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            SDL2
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];

          meta = with pkgs.lib; {
            mainProgram = "turrican2";
            description = "Port of the Amiga version of Turrican II (Factor 5, 1991)";
            license = licenses.gpl3Plus;
            platforms = platforms.linux;
          };
        };

        # R36S / ArkOS: eoan sysroot, see nix/r36s.nix.
        r36s = import ./nix/r36s.nix { inherit pkgs version; };
      in
      {
        packages.default = turrican2;
        packages.turrican2 = turrican2;
        packages.arkos-sysroot = r36s.sysroot;
        packages.turrican2-r36s = r36s.r36s;
        packages.turrican2-r36s-portmaster = r36s.portmaster;
        packages.turrican2-r36s-portmaster-zip = r36s.portmasterZip;

        apps.default = { type = "app"; program = "${turrican2}/bin/turrican2"; };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            pkg-config
            SDL2
            # useful for RE
            python3
            xxd
            file
            # m68k objdump for tools/disasm.py
            pkgsCross.m68k.buildPackages.binutils
          ];
          shellHook = ''
            echo "Turrican II dev shell"
            echo "  cmake -B build && cmake --build build"
            echo "  ./build/turrican2"
          '';
        };
      });
}
