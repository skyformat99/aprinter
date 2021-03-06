{ stdenv, fetchurl, pkgconfig, texinfo, perl, ncurses
, readline, gmp, mpfr, expat, zlib, python, guile, target }:
let
    version = "8.1";
    sha256 = "0d2bpqk58fqlx21rbnk8mbcjlggzc9kb5sjirrfrrrjq70ka0qdg";
in
stdenv.mkDerivation {
    name = "gdb-${target}-${version}";
    
    src = fetchurl {
        url = "mirror://gnu/gdb/gdb-${version}.tar.xz";
        inherit sha256;
    };
    
    nativeBuildInputs = [ pkgconfig texinfo perl ];
    
    buildInputs = [ ncurses readline gmp mpfr expat zlib python guile ];
    
    enableParallelBuilding = true;
    
    configureFlags = [
        "--target=${target}"
        "--with-gmp=${gmp.dev}" "--with-mpfr=${mpfr.dev}" "--with-system-readline"
        "--with-system-zlib" "--with-expat" "--with-libexpat-prefix=${expat.dev}"
    ];
    
    postInstall = ''
        rm -v $out/share/info/bfd.info
    '';
    
    doCheck = false;
}
