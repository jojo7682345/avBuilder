{
  description = "avBuilder - A flexible build system written in C";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    avUtils = {
      url = "github:jojo7682345/avUtils";
      flake=false;
    };
  };

  outputs = { self, nixpkgs, ... } @inputs:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      packages.${system}.avbuilder = pkgs.stdenv.mkDerivation rec {
        pname = "avBuilder";

        # Fetch the repo and preserve .git for version calculation
        #src = pkgs.fetchgit {
        #  url = "https://github.com/jojo7682345/avBuilder.git";
		#  sha256 = "sha256-TjySkoXpZ3sfE3EZNQyRhCaw9WYUMQng4UQNcx/0py4=";# Replace with actual hash
        #};
            src = pkgs.fetchgit {
            url = "https://github.com/jojo7682345/avBuilder.git";
            fetchSubmodules = true;
            sha256 = "sha256-AzUhrRYc6oEMIPGv5ab4fmtdGWc221c/c+nil+POcJA=";
        };

        # Compute version from git commit count and short hash
		version = "v0.1.001n";	
        
		nativeBuildInputs = with pkgs; [ 
			git 
			gcc
		];
		  #mkdir -p ./lib/AvUtils
		  #cp -r ${avUtils}/* ./lib/AvUtils/
		  #mkdir ./lib/AvUtils/build

        buildPhase = ''
		  chmod +x ./bootstrap
          ./bootstrap
		  mkdir build
        export AVBUILDER_HOME="./"
          ./avBuilder avBuilder.project
        '';

        installPhase = ''
          ./bootstrap install $out/bin/avBuilder

		  # Install your default projects somewhere inside the store
          ls
  		  mkdir -p $out/share/avBuilder/library/std/c
  		  cp ./library/std/c/stdc.project $out/share/avBuilder/library/std/c/

  		  mkdir -p $out/share/avBuilder/library/std/project
  		  cp ./library/std/project/import.project $out/share/avBuilder/library/std/project/	
        
		  mkdir -p $out/lib
		  cp ./lib/AvUtils/lib/*.a $out/lib/

		  mkdir -p $out/include
		  cp -r ./lib/AvUtils/include/* $out/include/
		'';
	
		shellHook = ''
			export AVBUILDER_HOME=$out/share/avBuilder
		'';
	
        meta = with pkgs.lib; {
          description = "A flexible build system written in C";
          homepage = "https://github.com/jojo7682345/avBuilder";
          license = licenses.mit;
          maintainers = with maintainers; [ ];
        };
      };

	  # Set the default package for `nix run .`
      defaultPackage.${system} = self.packages.${system}.avbuilder;

      # Set up a devShell for `nix develop .`
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = [ self.packages.${system}.avbuilder  pkgs.gdb ];
        shellHook = ''
          export AVBUILDER_HOME=${self.packages.${system}.avbuilder}/share/avBuilder
        '';
      };
    };
}

