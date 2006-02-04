Summary: A program transcode video files into Sony PSP mp4 format
Name: pspmovie
Epoch: 1
Version: 0.0.1
Release: 1
License: GPL
Group:  Applications/Multimedia
Source: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-root

BuildRequires: ffmpeg-devel >= 0.4.9
BuildRequires: faad2-devel >= 2.0
BuildRequires: qt-devel >= 3.3.5

Requires: faad2 >= 2.0
Requires: qt >= 3.3.5

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
./configure --disable-ffplay --disable-ffserver --disable-v4l --enable-gpl --enable-a52 --enable-faad
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"
cd ..
qmake pspmovie_static.pro
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin

install -s -m 755 pspmovie $RPM_BUILD_ROOT/usr/bin/pspmovie

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README TODO COPYING ChangeLog

%{_bindir}/pspmovie
