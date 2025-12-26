# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: No rights reserved

FROM ghcr.io/andyholmes/actuary:latest

RUN dnf install -y \
        --enablerepo=fedora-debuginfo,updates-debuginfo \
        --disablerepo=updates-testing,updates-testing-debuginfo \
        --setopt=install_weak_deps=False \
        glibc-langpack-en glibc-locale-source gi-docgen graphviz itstool rsync \
        yelp-tools \
        evolution-data-server-devel   evolution-data-server-debuginfo \
        glib2-devel                   glib2-debuginfo \
        glib-networking               glib-networking-debuginfo \
        glycin-loaders                glycin-loaders-debuginfo \
        gnutls-devel                  gnutls-debuginfo \
        gstreamer1-devel              gstreamer1-debuginfo \
        gstreamer1-plugins-base-devel gstreamer1-plugins-base-debuginfo \
        gstreamer1-plugins-good       gstreamer1-plugins-good-debuginfo \
        gtk4-devel                    gtk4-debuginfo \
        json-glib-devel               json-glib-debuginfo \
        libadwaita-devel              libadwaita-debuginfo \
        libei-devel                   libei-debuginfo \
        libpeas-devel                 libpeas-debuginfo \
        libphonenumber-devel          libphonenumber-debuginfo \
        libportal-devel               libportal-debuginfo \
        libportal-gtk4-devel          libportal-gtk4-debuginfo \
        libtinysparql                 libtinysparql-debuginfo \
        libxkbcommon-devel            libxkbcommon-debuginfo \
        pipewire-devel                pipewire-debuginfo \
        pulseaudio-libs-devel         pulseaudio-libs-debuginfo \
        sqlite-devel                  sqlite-debuginfo \
        tinysparql-devel              tinysparql-debuginfo && \
    dnf clean all && rm -rf /var/cache/dnf

# Build libwalbottle from source, since it's not available in most repositories
RUN git clone https://gitlab.com/walbottle/walbottle.git \
              --branch main \
              --single-branch && \
    cd walbottle && \
    meson setup --prefix=/usr \
                -Dgtk_doc=false \
                -Dinstalled_tests=false \
                -Dintrospection=disabled \
                -Dwerror=false \
                _build && \
    meson install -C _build

# Set the locale
ENV LANG=en_US.UTF-8
RUN localedef -c -i en_US -f UTF-8 en_US.UTF-8

