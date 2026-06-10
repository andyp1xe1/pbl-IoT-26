{
  description = "Air Glove companion — Vite PWA + Cloudflare Workers dev shell";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let pkgs = import nixpkgs { inherit system; };
      in {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            nodejs_22
            wrangler
          ];

          shellHook = ''
            echo "air-glove companion dev shell"
            echo "  npm install        # first time"
            echo "  npm run dev        # vite dev server (http://localhost:5173)"
            echo "  npm run build      # builds dist/ (PWA assets + service worker)"
            echo "  npm run cf:dev     # build + wrangler dev (local CF runtime)"
            echo "  wrangler login     # then: npm run deploy"
          '';
        };
      });
}
