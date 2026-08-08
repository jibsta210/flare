# Maintainer: Jake Steinman <j@metarealtyinc.ca>
pkgname=flare-screenshot-git
pkgver=0.4.0
pkgrel=1
pkgdesc="Screenshot tool for KDE Plasma Wayland that skips the XDG portal"
arch=('x86_64')
url="https://github.com/jibsta210/flare"
license=('MIT')
depends=('qt6-base' 'wl-clipboard')
makedepends=('git' 'gcc')
provides=('flare')
conflicts=('flare')
source=("git+https://github.com/jibsta210/flare.git")
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/flare"
  printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cd "$srcdir/flare"
  make
}

package() {
  cd "$srcdir/flare"
  # PREFIX must match the Exec= line in flare.desktop: KWin authorizes
  # ScreenShot2 by matching /proc/PID/exe against an installed desktop file.
  make DESTDIR="$pkgdir" PREFIX=/usr install
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
