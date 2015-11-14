name:           radard
version:        1.1.0
release:        1%{?dist}
summary:        ripple peer-to-peer network daemon

group:          applications/internet
license:        isc
url:            https://github.com/ripple/rippled

# curl -l -o sources/rippled-release.zip https://github.com/ripple/rippled/archive/release.zip
source0:        rippled-release.zip
buildroot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-xxxxxx)

buildrequires:  gcc-c++ scons openssl-devel protobuf-devel
requires:       protobuf openssl


%description
rippled is the server component of the ripple network.


%prep
%setup -n rippled-release


%build
# assume boost is manually installed
export rippled_boost_home=/usr/local/boost_1_55_0
scons -j `grep -c processor /proc/cpuinfo` build/rippled


%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/%{name}
cp license %{buildroot}/usr/share/%{name}/
mkdir -p %{buildroot}/usr/bin
cp build/rippled %{buildroot}/usr/bin/rippled
mkdir -p %{buildroot}/etc/%{name}
cp doc/rippled-example.cfg %{buildroot}/etc/%{name}/rippled.cfg
mkdir -p %{buildroot}/var/lib/%{name}/db
mkdir -p %{buildroot}/var/log/%{name}


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
/usr/bin/rippled
/usr/share/rippled/license
/etc/rippled/rippled-example.cfg
