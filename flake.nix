{
  description = "avBuilder - A flexible build system written in C";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      packages.${system}.avbuilder = pkgs.stdenv.mkDerivation rec {
        pname = "avBuilder";

        # Fetch the repo and preserve .git for version calculation
        src = pkgs.fetchgit {
          url = "https://github.com/jojo7682345/avBuilder.git";
		  sha256 = "sha256-2UkgzKbMnK4GREGKMpEYkjo4ac9N9pOqpYanp2VN2XA=";# Replace with actual hash
        };

        # Compute version from git commit count and short hash
		version = "v0.1.001n";	
        
		nativeBuildInputs = with pkgs; [ 
			git 
			gcc
		];

        buildPhase = ''
          chmod +x ./bootstrap
          ./bootstrap
          ./avBuilder avBuilder.project
        '';

        installPhase = ''
          ./bootstrap install $out/bin/avBuilder

		  # Install your default projects somewhere inside the store
  		  mkdir -p $out/share/avBuilder/library/std/c
  		  cp ./library/c/stdc.project $out/share/avBuilder/library/std/c/

  		  mkdir -p $out/share/avBuilder/library/std/project
  		  cp ./library/project/import.project $out/share/avBuilder/library/std/project/	
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

