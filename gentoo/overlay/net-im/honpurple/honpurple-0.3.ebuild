# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit eutils
DESCRIPTION="libpurple protocol plugin for Heroes of Newerth Chat Server"
HOMEPAGE="http://code.google.com/p/honpurple/"
SRC_URI="http://honpurple.googlecode.com/files/${P}.tar.bz2"

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="~x86 ~amd64"
IUSE=""

RDEPEND=">=net-im/pidgin-2.6.0
	>=dev-libs/glib-2.16"
DEPEND="${RDEPEND}"

S=${WORKDIR}

src_compile() {
	emake || die "emake failed."
}

src_install () {
	emake install || die "emake failed"
}
