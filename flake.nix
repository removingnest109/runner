{
  description = "runner — a TUI project script runner";

  # Pinned to nixos-24.11: that channel ships the exact dependency versions the
  # project's CMake asks for — ftxui 5.0.0, tomlplusplus 3.4.0, doctest 2.4.11.
  # runner needs the FTXUI 5.x API (it does not build against 7.x) and
  # find_package(ftxui 5.0.0) uses SameMajorVersion matching, so a channel whose
  # ftxui has moved to 6.x/7.x would not satisfy it without a code change. Run
  # `nix flake update` only when bumping to a channel that still carries ftxui 5.
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems =
        f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
      # Build the standalone package, then point its source at this flake's tree
      # instead of the fetchFromGitHub in packaging/nix/package.nix.
      runnerFor =
        pkgs:
        (pkgs.callPackage ./packaging/nix/package.nix { }).overrideAttrs (_: {
          src = self;
        });
    in
    {
      packages = forAllSystems (pkgs: rec {
        runner = runnerFor pkgs;
        default = runner;
      });

      apps = forAllSystems (pkgs: rec {
        runner = {
          type = "app";
          program = "${self.packages.${pkgs.system}.runner}/bin/runner";
        };
        default = runner;
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.system}.runner ];
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
