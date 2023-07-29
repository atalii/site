{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs }:
    let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
    in {
      devShell.x86_64-linux = import ./shell.nix { inherit pkgs; };
      packages.x86_64-linux.docker = import ./docker.nix { inherit pkgs; };
    };
}
