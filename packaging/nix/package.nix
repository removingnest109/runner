# Standalone package definition, written callPackage-style so it can be dropped
# into nixpkgs (pkgs/by-name/ru/runner/package.nix) almost verbatim. The flake
# calls this with `pkgs.callPackage` and overrides `src` to the local tree; a
# nixpkgs copy would use the fetchFromGitHub `src` below (fill in the hash).
{
  lib,
  stdenv,
  cmake,
  ftxui,
  tomlplusplus,
  doctest,
  fetchFromGitHub,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "runner";
  version = "0.1.0";

  # The flake overrides this with `self` (the checked-out tree). For a nixpkgs
  # submission, this fetchFromGitHub is the canonical source; replace the hash
  # with the real value (`nix-prefetch-url --unpack` or `nix flake prefetch`).
  src = fetchFromGitHub {
    owner = "removingnest109";
    repo = "runner";
    tag = "v${finalAttrs.version}";
    hash = lib.fakeHash;
  };

  nativeBuildInputs = [ cmake ];

  # FTXUI links into the binary (shared in nixpkgs → runtime closure); tomlplusplus
  # is header-only (build-time only, no runtime closure entry).
  buildInputs = [
    ftxui
    tomlplusplus
  ];

  # doctest is needed only to compile and run the test suite. Because it is a
  # nativeCheckInput and the test target is gated on doCheck (see cmakeFlags),
  # it never enters the runtime closure.
  nativeCheckInputs = [ doctest ];
  doCheck = true;

  cmakeFlags = [
    (lib.cmakeBool "RUNNER_BUILD_TESTS" finalAttrs.doCheck)
  ];

  meta = {
    description = "TUI project script runner";
    longDescription = ''
      runner reads a runner.toml describing a set of actions, shows them in an
      interactive terminal list, and runs the selected one in a child shell while
      streaming its output — including 256-color and truecolor ANSI — into a
      scrolling pane.
    '';
    homepage = "https://github.com/removingnest109/runner";
    license = lib.licenses.mit;
    mainProgram = "runner";
    platforms = lib.platforms.linux;
  };
})
