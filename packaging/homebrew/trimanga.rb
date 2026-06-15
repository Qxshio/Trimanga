class Trimanga < Formula
  desc "Clean manga archives by detecting removable scanlation and ad pages"
  homepage "https://github.com/Qxshio/Trimanga"
  url "https://github.com/Qxshio/Trimanga/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "REPLACE_WITH_RELEASE_TARBALL_SHA256"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "tesseract"
  depends_on "unzip"

  def install
    system "cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release",
           *std_cmake_args
    system "cmake", "--build", "build"
    bin.install "build/trimanga"
  end

  test do
    assert_match "Trimanga", shell_output("#{bin}/trimanga --version")
  end
end
