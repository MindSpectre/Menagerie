#!/usr/bin/env bash
#
# Menagerie Linux setup.
#
# Run from inside a checkout, it installs system dependencies and stops:
#
#     ./scripts/install-linux.sh
#
# Run standalone (piped from curl), it also clones the repository and produces a
# first build, so a bare machine gets from nothing to compiled in one command:
#
#     bash <(curl -fsSL https://raw.githubusercontent.com/MindSpectre/Menagerie/main/scripts/install-linux.sh)
#
# vcpkg itself is not handled here: cmake/vcpkg-bootstrap.cmake provisions it during
# the first configure.

set -euo pipefail

REPO_URL="https://github.com/MindSpectre/Menagerie.git"

TARGET_DIR=""
PRESET="dev-slim"
DO_BUILD=1
DEPS_ONLY=0

info()  { printf '\033[1;34m[info]\033[0m  %s\n' "$*"; }
warn()  { printf '\033[1;33m[warn]\033[0m  %s\n' "$*" >&2; }
error() { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage: install-linux.sh [options]

Installs the system packages Menagerie needs. When run outside a checkout it also
clones the repository, configures it, and builds it.

Options:
  --dir <path>      Clone into <path> (default: ./Menagerie). Standalone runs only.
  --preset <name>   Configure/build this preset (default: dev-slim).
  --no-build        Clone and configure, but skip the build.
  --deps-only       Install system packages and stop.
  -h, --help        Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dir)      TARGET_DIR="${2:-}"; [[ -n $TARGET_DIR ]] || error "--dir needs a path"; shift 2 ;;
        --preset)   PRESET="${2:-}";     [[ -n $PRESET ]]     || error "--preset needs a name"; shift 2 ;;
        --no-build) DO_BUILD=0; shift ;;
        --deps-only) DEPS_ONLY=1; shift ;;
        -h|--help)  usage; exit 0 ;;
        *)          usage >&2; error "unknown option: $1" ;;
    esac
done

# ── dnf (Fedora/RHEL) ───────────────────────────────────────────────────────

install_dnf() {
    info "Installing system packages via dnf..."

    sudo dnf -y group install development-tools

    sudo dnf -y install \
        clang \
        clang-tools-extra \
        libcxx-devel \
        libcxxabi-devel \
        perl-IPC-Cmd \
        kernel-headers \
        curl \
        openssl-devel \
        bison \
        flex \
        make \
        cmake \
        autoconf \
        autoconf-archive \
        automake \
        libtool \
        ninja-build \
        ccache \
        mold \
        git \
        zip \
        unzip \
        tar

    sudo dnf -y install perl-CPAN
    cpan "IPC::Cmd"
}

# ── apt (Debian/Ubuntu) ─────────────────────────────────────────────────────

install_apt() {
    info "Installing system packages via apt..."

    sudo apt-get update

    sudo apt-get install -y \
        build-essential \
        clang \
        clang-tools \
        libc++-dev \
        libc++abi-dev \
        libperl-dev \
        linux-headers-generic \
        curl \
        libssl-dev \
        bison \
        flex \
        make \
        cmake \
        autoconf \
        autoconf-archive \
        automake \
        libtool \
        ninja-build \
        ccache \
        mold \
        pkg-config \
        git \
        zip \
        unzip \
        tar

    sudo apt-get install -y libipc-run-perl
}

# ── pacman (Arch/Manjaro) ───────────────────────────────────────────────────

install_pacman() {
    info "Installing system packages via pacman..."

    sudo pacman -Syu --noconfirm

    sudo pacman -S --needed --noconfirm \
        base-devel \
        clang \
        libc++ \
        libc++abi \
        linux-headers \
        curl \
        openssl \
        bison \
        flex \
        make \
        cmake \
        autoconf \
        autoconf-archive \
        automake \
        libtool \
        ninja \
        ccache \
        mold \
        pkgconf \
        git \
        zip \
        unzip \
        perl
}

install_packages() {
    if command -v dnf &>/dev/null; then
        install_dnf
    elif command -v apt-get &>/dev/null; then
        install_apt
    elif command -v pacman &>/dev/null; then
        install_pacman
    else
        error "Unsupported package manager. Supported: dnf (Fedora/RHEL), apt (Debian/Ubuntu), pacman (Arch)."
    fi
}

# ── Locate the checkout, if we are in one ───────────────────────────────────
#
# Piped invocations (`bash <(curl ...)`, `curl ... | bash`) leave BASH_SOURCE
# pointing at a pipe or nothing at all, so both land in standalone mode.

find_checkout() {
    local script_path="${BASH_SOURCE[0]:-}"
    [[ -n $script_path && -f $script_path ]] || return 1

    local script_dir root
    script_dir="$(cd -- "$(dirname -- "$script_path")" && pwd)" || return 1
    root="$(cd -- "$script_dir/.." && pwd)" || return 1

    [[ -f "$root/vcpkg.json" && -f "$root/CMakePresets.json" ]] || return 1
    printf '%s' "$root"
}

# ── Clone ───────────────────────────────────────────────────────────────────

clone_repo() {
    local dest="$1"

    if [[ -e $dest ]]; then
        if [[ -d "$dest/.git" && -f "$dest/vcpkg.json" && -f "$dest/CMakePresets.json" ]]; then
            info "Reusing existing checkout at $dest"
            return 0
        fi
        error "$dest already exists and is not a Menagerie checkout. Move it aside or pass --dir <path>."
    fi

    info "Cloning $REPO_URL into $dest..."
    git clone "$REPO_URL" "$dest"
}

# ── Run ─────────────────────────────────────────────────────────────────────
#
# Work out the plan and announce it before installing anything, so the sudo
# prompts that follow are not a surprise.

CHECKOUT="$(find_checkout || true)"

if [[ $DEPS_ONLY -eq 1 ]]; then
    info "Plan: install system packages, then stop (--deps-only)."
elif [[ -n $CHECKOUT ]]; then
    info "Plan: install system packages for the checkout at $CHECKOUT."
else
    [[ -n $TARGET_DIR ]] || TARGET_DIR="${PWD%/}/Menagerie"
    if [[ $DO_BUILD -eq 1 ]]; then
        info "Plan: install system packages, clone into $TARGET_DIR, then configure and build '$PRESET'."
    else
        info "Plan: install system packages, clone into $TARGET_DIR, then configure '$PRESET' (--no-build)."
    fi
fi

install_packages

if [[ $DEPS_ONLY -eq 1 ]]; then
    info "System packages installed (--deps-only)."
    exit 0
fi

if [[ -n $CHECKOUT ]]; then
    info "Running inside $CHECKOUT — system packages installed."
    info "Next: cmake --preset $PRESET && cmake --build --preset $PRESET"
    exit 0
fi

# Standalone: clone, configure, build.
clone_repo "$TARGET_DIR"
cd "$TARGET_DIR"

info "Configuring preset '$PRESET' (this provisions vcpkg and builds its ports — expect several minutes)..."
cmake --preset "$PRESET"

if [[ $DO_BUILD -eq 0 ]]; then
    info "Configured. Skipping build (--no-build)."
    info "Next: cd $TARGET_DIR && cmake --build --preset $PRESET"
    exit 0
fi

info "Building preset '$PRESET'..."
cmake --build --preset "$PRESET"

info "Done. Menagerie is at $TARGET_DIR"
info "Run the tests with: cd $TARGET_DIR && ctest --preset $PRESET"
