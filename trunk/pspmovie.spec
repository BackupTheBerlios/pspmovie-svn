Summary: A program transcode video files into Sony PSP mp4 format
Name: pspmovie
Version: 0.0.1
Release: 1
Copyright: GPL
Group:  Applications/Multimedia
Source: pspmovie-0.0.1.tar.gz
Patch: pspmovie-0.0.1-buildroot.patch
BuildRoot: /var/tmp/%{name}-buildroot

%description
The pspmovie allows the user to transcode video files into
format that Sony PSP understands. It can also help to transfer
resulting movie files into PSP connected to USB port.

Install pspmovie if you'd like to watch movies on your PSP
device.


%prep
%setup -q
%patch -p1 -b .buildroot

%build
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/man/man1

install -s -m 755 pspmovie $RPM_BUILD_ROOT/usr/bin/pspmovie
install -m 644 pspmovie.1 $RPM_BUILD_ROOT/usr/man/man1/pspmovie.1

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc README TODO COPYING ChangeLog

/usr/bin/pspmovie
/usr/man/man1/pspmovie.1
