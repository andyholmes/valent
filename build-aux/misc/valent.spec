%global tarball_version %%(echo %{version} | tr '~' '.')

%global glib2_version >= 2.72.0
%global gtk4_version >= 4.8.0
%global json_glib_version >= 1.6.0
%global libpeas_version >= 1.22.0
%global libeds_version >= 3.34.0
%global sqlite_version >= 3.24.0
%global libadwaita_version >= 1.2.0
%global libportal_version >= 0.5, pkgconfig(libportal) <= 0.6

Name:           valent
Version:        1.0.0~alpha
Release:        1%{?dist}
Summary:        Connect, control and sync devices

License:        GPLv3+
URL:            https://github.com/andyholmes/%{name}
Source0:        %{url}/archive/v%{version}/%{name}-%{tarball_version}.tar.gz

BuildRequires:  firewalld-filesystem
BuildRequires:  gcc
BuildRequires:  gettext
BuildRequires:  meson
BuildRequires:  pkgconfig(gio-2.0) %{glib2_version}
BuildRequires:  pkgconfig(gio-unix-2.0) %{glib2_version}
BuildRequires:  pkgconfig(gnutls)
BuildRequires:  pkgconfig(gtk4) %{gtk4_version}
BuildRequires:  pkgconfig(json-glib-1.0) %{json_glib_version}
BuildRequires:  pkgconfig(libadwaita-1) %{libadwaita_version}
BuildRequires:  pkgconfig(libebook-1.2) %{libeds_version}
BuildRequires:  pkgconfig(libpeas-1.0) %{libpeas_version}
BuildRequires:  pkgconfig(libportal) %{libportal_version}
BuildRequires:  pkgconfig(sqlite3) %{sqlite_version}
BuildRequires:  pkgconfig(gstreamer-1.0)
# TODO: For `photo` plugin
BuildRequires:  pkgconfig(gstreamer-video-1.0)
# For `pulseaudio` plugin
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(libpulse-mainloop-glib)
BuildRequires:  %{_bindir}/desktop-file-validate
BuildRequires:  %{_bindir}/appstream-util

Requires:       glib2%{?_isa} %{glib2_version}
Requires:       gtk4%{?_isa} %{gtk4_version}
Requires:       json-glib%{?_isa} %{json_glib_version}
Requires:       libpeas%{?_isa} %{libpeas_version}
Requires:       evolution-data-server%{?_isa} %{libeds_version}
Requires:       gnutls%{?_isa}

Recommends:     libpeas-loader-python3%{?_isa} %{libpeas_version}
Recommends:     pkgconfig(libportal-gtk4) %{libportal_version}

%description
Securely connect your devices to open files and links where you need them, get
notifications when you need them, stay in control of your media and more.

%package        devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing plugins for %{name}.

%package        tests
Summary:        Installed tests for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    tests
The %{name}-tests package contains precompiled unit tests for %{name}

%prep
%autosetup -p1 -n %{name}-%{tarball_version}

%build
%meson --buildtype=release -Ddocumentation=true -Dfirewalld=true -Dtests=true -Dfuzz_tests=false
%meson_build

%install
%meson_install
%find_lang %{name}

%check
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/*.xml
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop

%post
%firewalld_reload

%files -f %{name}.lang
%doc CHANGELOG.md README.md
%license LICENSE
%{_bindir}/valent
%{_datadir}/applications/ca.andyholmes.Valent.desktop
%{_datadir}/dbus-1/services/ca.andyholmes.Valent.service
%{_datadir}/glib-2.0/schemas/ca.andyholmes.Valent*.gschema.xml
%{_datadir}/icons/hicolor/scalable/apps/ca.andyholmes.Valent.svg
%{_datadir}/icons/hicolor/symbolic/apps/ca.andyholmes.Valent-symbolic.svg
%{_datadir}/metainfo/ca.andyholmes.Valent.metainfo.xml
%{_prefix}/lib/firewalld/services/ca.andyholmes.Valent.xml
%{_libdir}/girepository-1.0/
%{_libdir}/libvalent-1.so*
%{_sysconfdir}/xdg/autostart/ca.andyholmes.Valent-autostart.desktop

%files devel
%{_datadir}/gir-1.0/
%{_datadir}/doc/
%{_datadir}/vala/vapi/
%{_libdir}/pkgconfig/
%{_includedir}/libvalent-1/

%files tests
%{_datadir}/installed-tests/libvalent-1/
%{_libexecdir}/installed-tests/libvalent-1/

%changelog
* Thu Mar 23 2023 Andy Holmes <andrew.g.r.holmes@gmail.com> - 1.0.0~alpha

- Initial packaging

