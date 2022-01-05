%global glib2_version 2.66.0
%global gtk4_version 4.4.0
%global json_glib_version 1.6.0
%global libpeas_version 1.22.0
%global libeds_version 3.34.0
%global sqlite_version 3.24.0

Name:           valent
Version:        0.1.0
Release:        1%{?dist}
Summary:        Connect, control and sync devices

License:        GPLv3+
URL:            https://github.com/andyholmes/%{name}
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  firewalld-filesystem
BuildRequires:  gcc
BuildRequires:  gettext
BuildRequires:  meson
BuildRequires:  pkgconfig(gio-2.0) >= %{glib2_version}
BuildRequires:  pkgconfig(gio-unix-2.0) >= %{glib2_version}
BuildRequires:  pkgconfig(glib-2.0) >= %{glib2_version}
BuildRequires:  pkgconfig(gnutls)
BuildRequires:  pkgconfig(gtk4) >= %{gtk4_version}
BuildRequires:  pkgconfig(libadwaita-1)
BuildRequires:  pkgconfig(json-glib-1.0) >= %{json_glib_version}
BuildRequires:  pkgconfig(libpeas-1.0) >= %{libpeas_version}
BuildRequires:  pkgconfig(libebook-1.2) >= %{libeds_version}
BuildRequires:  pkgconfig(libebook-contacts-1.2) >= %{libeds_version}
BuildRequires:  pkgconfig(libedata-book-1.2) >= %{libeds_version}
BuildRequires:  pkgconfig(libedataserver-1.2) >= %{libeds_version}
BuildRequires:  pkgconfig(sqlite3) >= %{sqlite_version}
BuildRequires:  pkgconfig(gstreamer-1.0)
# TODO: For `photo` plugin
BuildRequires:  pkgconfig(gstreamer-video-1.0)
# For `pulseaudio` plugin
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(libpulse-mainloop-glib)
BuildRequires:  %{_bindir}/desktop-file-validate
BuildRequires:  %{_bindir}/appstream-util
# For `xdp` plugin
BuildRequires:  pkgconfig(libportal)

Requires:       glib2%{?_isa} >= %{glib2_version}
Requires:       gtk4%{?_isa} >= %{gtk4_version}
Requires:       json-glib%{?_isa} >= %{json_glib_version}
Requires:       libpeas%{?_isa} >= %{libpeas_version}
Requires:       libpeas-loader-python3%{?_isa} >= %{libpeas_version}
Requires:       evolution-data-server%{?_isa} >= %{libeds_version}
Requires:       gnutls%{?_isa}

%description
Securely connect your devices to open files and links where you need them, get
notifications when you need them, stay in control of your media and more.

%package        devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing plugins for %{name}.

%prep
%autosetup -p1

%build
%meson --buildtype=release -Ddocumentation=true -Dfirewalld=true
%meson_build

%install
%meson_install
%find_lang %{name}

%check
%meson_test
#desktop-file-validate %{buildroot}%{_datadir}/applications/ca.andyholmes.Valent.desktop
#appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/ca.andyholmes.Valent.appdata.xml

%post
%firewalld_reload

%files -f %{name}.lang
%doc CHANGELOG.md README.md
%license LICENSE
%{_bindir}/valent
%exclude %{_datadir}/gir-1.0/
%exclude %{_datadir}/doc/
%{_datadir}/applications/ca.andyholmes.Valent.desktop
%{_datadir}/dbus-1/services/ca.andyholmes.Valent.service
%{_datadir}/glib-2.0/schemas/ca.andyholmes.valent*.gschema.xml
%{_datadir}/icons/hicolor/scalable/apps/ca.andyholmes.Valent.svg
%{_datadir}/icons/hicolor/symbolic/apps/ca.andyholmes.Valent-symbolic.svg
%{_datadir}/metainfo/ca.andyholmes.Valent.metainfo.xml
%{_prefix}/lib/firewalld/services/ca.andyholmes.Valent.xml
%exclude %{_libdir}/pkgconfig/
%{_libdir}/girepository-1.0/
%{_libdir}/libvalent.so*

%files devel
%{_datadir}/gir-1.0/
%{_datadir}/doc/
%{_libdir}/pkgconfig/
%{_includedir}/valent*/

%changelog
* Tue Jan 4 2021 Andy Holmes <andrew.g.r.holmes@gmail.com> - 0.1.0-1

- Initial packaging

