{ pkgs ? import <nixpkgs> {} }:



let
  name = "atalii/talinet";
  version = "23072803";

  package = import ./default.nix { inherit pkgs; };
in pkgs.dockerTools.buildImage {
  name = name;
  tag = version;

  copyToRoot = pkgs.buildEnv {
    name = "${name}-root";
    paths = [ package ];
    pathsToLink = [ "/bin" ];
  };

  config.Cmd = [ "/bin/srv" ];
  config.Expose = [ 8080 ];
}
