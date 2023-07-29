{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [ libarchive gnumake gcc zip unzip ];
}
