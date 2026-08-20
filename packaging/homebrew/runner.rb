class Runner < Formula
  desc "TUI project script runner"
  homepage "https://github.com/removingnest109/runner"
  url "https://github.com/removingnest109/runner/archive/refs/tags/v0.2.0.tar.gz"
  sha256 "4fd73f6f0088be0d661c254165feb4dbcb5974eb97fa4f17e3b2d11806da9bc9"
  license "MIT"

  # runner needs the FTXUI 6.0+ selection API and builds against 6.x and 7.x;
  # Homebrew's ftxui (7.x) satisfies find_package(ftxui). tomlplusplus is
  # header-only, so it is build-only. The test suite (doctest) is skipped here in
  # favor of the `test do` smoke check below, keeping the build dependency-light.
  depends_on "cmake" => :build
  depends_on "tomlplusplus" => :build
  depends_on "ftxui"

  def install
    system "cmake", "-S", ".", "-B", "build",
           "-DRUNNER_BUILD_TESTS=OFF", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match "runner #{version}", shell_output("#{bin}/runner --version")

    # --generate-config writes a starter runner.toml and refuses to clobber it.
    system bin/"runner", "--generate-config"
    assert_predicate testpath/"runner.toml", :exist?
  end
end
