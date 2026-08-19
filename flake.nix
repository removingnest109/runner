{
  description = "runner — a TUI project script runner";

  # Pinned to nixos-25.05: that channel ships ftxui 6.x (6.1.9), tomlplusplus
  # 3.4.0, and doctest 2.4.11. runner needs the FTXUI 6.0+ selection API and
  # builds against both 6.x and 7.x (its CMake queries find_package(ftxui)
  # unversioned with a 6.0 floor), so any channel carrying ftxui >= 6 works. Run
  # `nix flake update` only when bumping to a channel that still carries ftxui >= 6.
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";

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
