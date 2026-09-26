{
  description = "XsetWall - the sexy tool to set sexy wallpapers for X11";

  inputs.nixpkgs.url = "https://channels.nixos.org/nixos-26.05/nixexprs.tar.zst";

  outputs =
    { self, nixpkgs }:
    let
      inherit (nixpkgs) lib;

      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      # xsetwall.c is the single source of truth for the version, so read it
      # from there instead of inventing one from the flake's timestamp.
      version =
        let
          matched = lib.match ''.*"v([0-9]+\.[0-9]+\.[0-9]+)".*'' (
            lib.findFirst (line: lib.hasInfix "XSETWALL_VERSION" line) "" (
              lib.splitString "\n" (builtins.readFile ./xsetwall.c)
            )
          );
        in
        if matched == null then
          throw "xsetwall: could not read the version out of xsetwall.c"
        else
          lib.head matched;

      # Only the files the build actually reads. Keeps stray artifacts (the
      # compiled binaries, editor backups, ...) from changing the hash.
      src = lib.fileset.toSource {
        root = ./.;
        fileset = lib.fileset.unions [
          ./xsetwall.c
          ./stb_image.h
          ./xsetwall.1
          ./readme
          ./LICENSE
        ];
      };

      # stdenv is a parameter so the same recipe builds the glibc and the musl
      # package from pkgs and pkgs.pkgsMusl.
      mkXsetwall =
        pkgs:
        pkgs.stdenv.mkDerivation {
          pname = "xsetwall";
          inherit src version;

          strictDeps = true;
          dontConfigure = true;
          hardeningEnable = [
            "bindnow"
            "format"
            "fortify"
            "pic"
            "relro"
            "stackprotector"
          ];

          buildInputs = [ pkgs.libx11 ];

          env.NIX_CFLAGS_COMPILE = "-O3 -Wall -Wextra";

          buildPhase = ''
            runHook preBuild
            $CC $NIX_CFLAGS_COMPILE -o xsetwall xsetwall.c -lX11 -lm
            runHook postBuild
          '';

          doCheck = true;
          checkPhase = ''
            runHook preCheck

            ./xsetwall --version | grep -q "XsetWall v${version}"

            ./xsetwall --help | grep -q -- "--white-borders"

            # no arguments -> usage on stderr, exit 64
            usage=$(./xsetwall 2>&1) && status=0 || status=$?
            test "$status" -eq 64
            echo "$usage" | grep -q "usage:"

            # unparsable scale -> exit 65
            ./xsetwall -s nope >/dev/null 2>&1 && status=0 || status=$?
            test "$status" -eq 65

            runHook postCheck
          '';

          installPhase = ''
            runHook preInstall
            install -Dm755 xsetwall $out/bin/xsetwall
            install -Dm644 xsetwall.1 $out/share/man/man1/xsetwall.1
            install -Dm644 readme $out/share/doc/xsetwall/readme
            install -Dm644 LICENSE $out/share/doc/xsetwall/LICENSE
            runHook postInstall
          '';

          meta = {
            description = "Set X11 wallpapers from the command line";
            longDescription = ''
              A small C utility that sets a desktop wallpaper on an X11
              display. Images are scaled to fit the screen while preserving
              the aspect ratio, and can be aligned or padded however you like.
            '';
            homepage = "https://github.com/0x61nas/xsetwall";
            license = lib.licenses.mit;
            mainProgram = "xsetwall";
            platforms = systems;
          };
        };

      # End-to-end test: run the real binary against a real (headless) X server
      # and a tiny hand-rolled PPM, which stb_image can decode.
      e2eCheck =
        pkgs: xsetwall:
        pkgs.runCommand "xsetwall-e2e" { nativeBuildInputs = [ pkgs.xvfb-run ]; } ''
          printf 'P6\n2 2\n255\n' > red.ppm
          printf '\377\0\0\377\0\0\377\0\0\377\0\0' >> red.ppm

          xvfb-run -a ${xsetwall}/bin/xsetwall -s 2 red.ppm > log.txt
          cat log.txt
          grep -q "image: 2x2" log.txt
          grep -q "scaled: 4x4" log.txt

          touch $out
        '';

    in
    {
      packages = lib.genAttrs systems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          xsetwall = mkXsetwall pkgs;
          musl = mkXsetwall pkgs.pkgsMusl;
          default = self.packages.${system}.xsetwall;
        }
      );

      apps = lib.genAttrs systems (system: {
        default = {
          type = "app";
          program = lib.getExe self.packages.${system}.xsetwall;
          meta.description = "Set an X11 wallpaper";
        };
      });

      checks = lib.genAttrs systems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          xsetwall = self.packages.${system}.xsetwall;
        in
        {
          inherit xsetwall;
        }
        // lib.optionalAttrs pkgs.stdenv.isLinux {
          x11 = e2eCheck pkgs xsetwall;
        }
      );

      devShells = lib.genAttrs systems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            name = "xsetwall-${version}";

            # the compiler + libX11 headers of the package itself, so the shell
            # can never drift from what the package builds against
            inputsFrom = [ (mkXsetwall pkgs) ];

            packages = with pkgs; [
              gdb
              valgrind
              shellcheck
              xprop
              xdpyinfo
              xvfb-run # for testing without a real display
              nixfmt
            ];

            shellHook = ''
              echo "xsetwall ${version} | ./x build | nix build | nix flake check | nix fmt"
            '';
          };
        }
      );

      formatter = lib.genAttrs systems (system: nixpkgs.legacyPackages.${system}.nixfmt);
    };
}
