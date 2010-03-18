 #!/bin/bash
 # author: David Thomas (davidt@catalyst.net.nz)
 # author: Neil Bertram (neil@catalyst.net.nz)
 # version bump a debianized git checkout
 # copy and run the script from your drupal dir or install it
 # Requires dctrl-tools to be installed
 # usage: ./bump.sh [<version>]

 # Die on error
 set -e

 if [ $2 ]
 then
   echo -e "Usage: bump.sh [<version>]\nNote that the version number is normally calculated automatically"
   exit 1;
 fi

 VERSION=$1

 # Autodetect package from the Debian dir
 if [ ! -e debian ]
 then
   echo "Can\'t find the ./debian dir, please run this from the checkout root"
   exit 1
 fi
 PACKAGE=`grep-dctrl -ns Package -P drupal debian/control`
 if [ ! $PACKAGE ]
 then
   echo "Unable to find package name in debian/control file?!?"
   exit 1
 fi

 if [ ! $VERSION ]
 then
   # We make one up in YYYYMMDD-X format, where X is the previous revision + 1
   BASEDATE=`date +%Y%m%d-`
   SUFFIX=1
   while true
   do
     # Does this release already exist?
     if grep -qs "$BASEDATE$SUFFIX" debian/changelog
     then
       let SUFFIX=SUFFIX+1
     else
       VERSION="$BASEDATE$SUFFIX"
       break
     fi
   done
 fi

 dch -v $VERSION
 CHANGELOGMSG=`dpkg-parsechangelog --count 1 | grep -A 20 Changes | tail -n +4`
 git add debian/changelog
 COMMITMSG=`echo -e "Automatic release bump to $VERSION:\n\n$CHANGELOGMSG"`
 git commit -m "$COMMITMSG"
 git tag -f $PACKAGE-$VERSION
 git push
 git push origin tag $PACKAGE-$VERSION
