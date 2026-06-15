class ScanlationTool < Formula
  desc "Standalone scanlation and ad page detector for manga archives"
  homepage "https://github.com/example/scanlation-tool"
  url "https://github.com/example/scanlation-tool/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "REPLACE_WITH_RELEASE_TARBALL_SHA256"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "tesseract"
  depends_on "unzip"

  def install
    system "cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release",
           *std_cmake_args
    system "cmake", "--build", "build"
    bin.install "build/scanlation_tool"
  end

  test do
    assert_match "scanlation_tool", shell_output("#{bin}/scanlation_tool --version")
  end
end
