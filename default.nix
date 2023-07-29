{ pkgs ? import <nixpkgs> {} }:


pkgs.stdenv.mkDerivation {
  pname = "talinetwork";
  version = "0.0.0";

  src = ./.;

  buildInputs = with pkgs; [ libarchive zip ];

  buildPhase = ''
    runHook preBild

    make -j$NIX_BUILD_CORES

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin/
    cp server $out/bin/srv

    runHook postInstall
  '';

  dontConfigure = true;
  dontStrip = true;
}
