%define commit %(git rev-parse HEAD)
%define shortcommit %(git rev-parse --short=8 HEAD)

Summary: University of Wisconsin IPMI Multitool
Name: ipmimt
Version: 0.1.0
Release: 1%{?dist}
#Release: 1%{?dist}.%(git rev-parse --abbrev-ref HEAD | sed s/-/_/g)
#BuildArch: %{_buildarch}
License: Reserved
Group: Applications/XDAQ
#Source: http://github.com/uwcms/sysmgr-tools/archive/%{commit}/sysmgr-tools-%{commit}.tar.gz
URL: https://github.com/uwcms/sysmgr-tools
BuildRoot: %{PWD}/rpm/buildroot
Requires: sysmgr >= 1.1.0, boost-program-options >= 1.41.0
#Prefix: /usr

%description
The University of Wisconsin IPMI Multitool provides an interface for performing
IPMI-related queries and maintenance through the University of Wisconsin System
Manager.

#
# Devel RPM specified attributes (extension to binary rpm with include files)
#
#%package -n ipmimt-devel
#Summary: University of Wisconsin IPMI Multitool
#Group:    Applications/XDAQ
#
#%description -n %{_project}-%{_packagename}-devel
#The University of Wisconsin IPMI Multitool provides an interface for performing
#IPMI-related queries and maintenance through the University of Wisconsin System
#Manager.

#%prep

#%setup 

#%build

#
# Prepare the list of files that are the input to the binary and devel RPMs
#
%install
mkdir -p %{buildroot}/usr/bin
#mkdir -p %{buildroot}/usr/share/doc/%{name}-%{version}/

install -m 755 $IPMIMT_ROOT/ipmimt %{buildroot}/usr/bin/
#install -m 655 %{_packagedir}/MAINTAINER %{_packagedir}/rpm/RPMBUILD/BUILD/MAINTAINER

%clean
rm -rf %{buildroot}

#
# Files that go in the binary RPM
#
%files
%defattr(-,root,root,-)
#%doc /usr/share/doc/%{name}-%{version}/
/usr/bin/ipmimt

#
# Files that go in the devel RPM
#
#%files -n ipmimt-devel
#%defattr(-,root,root,-)

#%changelog

%debug_package
