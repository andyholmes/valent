FROM fedora:34

# The packages below fall roughly into three categories: the first line of
# packages is build tooling, the second gi-docgen dependencies, third and fourth
# test tools and the pairs with debuginfo are dependencies of Valent
RUN dnf install -y --enablerepo=fedora-debuginfo,updates-debuginfo \
        clang gcc gettext git libasan libubsan meson \
        python3-jinja2 python3-markdown python3-toml python3-tyopgrify \
        appstream desktop-file-utils dbus-daemon lcov python-dbusmock rsync \
        xorg-x11-server-Xvfb \
        alsa-lib-devel                alsa-lib-debuginfo \
        evolution-data-server-devel   evolution-data-server-debuginfo \
        glib2-devel                   glib2-debuginfo \
        gnutls-devel                  gnutls-debuginfo \
        gstreamer1-devel              gstreamer1-debuginfo \
        gstreamer1-plugins-base-devel gstreamer1-plugins-base-debuginfo \
        gstreamer1-plugins-good       gstreamer1-plugins-good-debuginfo \
        gtk4-devel                    gtk4-debuginfo \
        json-glib-devel               json-glib-debuginfo \
        libpeas-devel                 libpeas-debuginfo \
        libportal-devel               libportal-debuginfo \
        pulseaudio-libs-devel         pulseaudio-libs-debuginfo \
        sqlite-devel                  sqlite-debuginfo && \
    dnf clean all && rm -rf /var/cache/dnf

