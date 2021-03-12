EAPI=7

DESCRIPTION="lpmd small power management daemon for linux laptops"
HOMEPAGE="https://github.com/adippl/lpmd"
SRC_URI=""

inherit git-r3
EGIT_REPO_URI="https://github.com/adippl/lpmd"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

DEPEND=""
RDEPEND="${DEPEND}"
BDEPEND=""

src_install() {
	mkdir -p ${D}/usr/bin/
	mkdir -p ${D}/etc/init.d/
	cp ${S}/lpmd ${D}/usr/bin/lpmd
	cp ${S}/lpmd-openrc ${D}/etc/init.d/lpmd
	chmod +x ${D}/usr/bin/lpmd ${D}/etc/init.d/lpmd

}

