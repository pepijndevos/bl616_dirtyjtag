# Nix development shell for the DirtyJTAG BL702 port.
#
# Provides the Xuantie/T-Head RISC-V bare-metal cross-toolchain (the patched
# GCC 10.2.0 that bouffalo_sdk requires, with CLIC/mtvt CSR support) plus
# cmake/make/git.
#
# The host toolchain (pkgs.stdenv.cc) is for the PC-side regression tests in
# tests/, which build against fakes rather than the RISC-V SDK.
{ pkgs ? import <nixpkgs> {} }:

let
  xuantieToolchain = pkgs.stdenv.mkDerivation {
    name = "xuantie-riscv64-toolchain";
    src = ./toolchain;

    nativeBuildInputs = [ pkgs.autoPatchelfHook ];
    buildInputs = [
      pkgs.stdenv.cc.cc.lib
      pkgs.zlib
      pkgs.ncurses5
    ];

    installPhase = ''
      runHook preInstall
      mkdir -p $out
      cp -r * $out/
      runHook postInstall
    '';
  };

in
pkgs.mkShell {
  nativeBuildInputs = [
    xuantieToolchain
    pkgs.cmake
    pkgs.gnumake
    pkgs.git
    pkgs.clang-tools  # provides clang-format for the style check
    pkgs.stdenv.cc    # host gcc for tests/ (AddressSanitizer included)
    pkgs.blisp        # BL702 ROM bootloader flashing tool for make upload
  ];

  hardeningDisable = [ "all" ];

  # Nix sets CONFIG_SHELL (for autoconf) to the Nix bash path; the SDK's
  # Makefile treats any CONFIG_* variable as a Kconfig option and writes it
  # to autoconf.h, breaking the IS_ENABLED() macro. Unset it.
  shellHook = ''
    unset CONFIG_SHELL
  '';
}
