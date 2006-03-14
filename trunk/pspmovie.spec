Summary: A program transcode video files into Sony PSP mp4 format
Name: pspmovie
Version: 0.0.2
Release: 1%{?dist}
License: GPL
Group:  Applications/Multimedia
Source: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: faad2-devel >= 2.0
BuildRequires: faac-devel >= 1.24
BuildRequires: qt-devel >= 3.3.5, qt-designer
BuildRequires: dbus-devel >= 0.33
BuildRequires: desktop-file-utils


Requires: faad2 >= 2.0
Requires: faac >= 1.24
Requires: qt >= 3.3.5
Requires: dbus >= 0.33

%description
The pspmovie allows the user to transcode video files into
format that Sony PSP understands. It can also help to transfer
resulting movie files into PSP connected to USB port.

Install pspmovie if you'd like to watch movies on your PSP
device.


%prep
%setup -q

%build
cd ffmpeg
./configure --disable-ffplay --disable-ffserver --disable-v4l --enable-gpl --enable-a52 --enable-faad --enable-faac --disable-vhook --disable-network
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"
cd ..
qmake pspmovie.pro
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT%{_datadir}/applications
mkdir -p $RPM_BUILD_ROOT%{_datadir}/pixmaps

install -p -s -m 755 pspmovie $RPM_BUILD_ROOT%{_bindir}/pspmovie
install -p -m 644 pspmovie.desktop $RPM_BUILD_ROOT%{_datadir}/applications/pspmovie.desktop
install -p -m 644 pspmovie.png $RPM_BUILD_ROOT%{_datadir}/pixmaps/pspmovie.png

desktop-file-install --vendor fedora --delete-original      \
  --dir $RPM_BUILD_ROOT%{_datadir}/applications             \
  --add-category X-Red-Hat-Extra                            \
  $RPM_BUILD_ROOT%{_datadir}/applications/pspmovie.desktop


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README TODO COPYING ChangeLog

%{_bindir}/pspmovie
%{_datadir}/applications/*pspmovie.desktop
%{_datadir}/pixmaps/pspmovie.png
