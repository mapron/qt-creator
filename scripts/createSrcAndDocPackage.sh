#!/bin/bash

## Command line parameters
if [[ $# != 2 ]]; then
    cat <<USAGE
usage:
  $0 <branch> <version>

  Creates tar and zip source package from origin/<branch> and documentation-zip from current checkout.
  Files and directories are named after <version>.
  example:
    $0 2.0.0 2.0.0-rc1
USAGE
    exit 1
fi

BRANCH=$1
VERSION=$2
cd `dirname $0`/..
echo "Creating tar archive..."
git archive --format=tar --prefix=qt-creator-${VERSION}-src/ origin/${BRANCH} | gzip > qt-creator-${VERSION}-src.tar.gz || exit 1
echo "Creating zip archive..."
git archive --format=zip --prefix=qt-creator-${VERSION}-src/ origin/${BRANCH} > qt-creator-${VERSION}-src.zip || exit 1
echo "Creating documentation..."
rm -r doc/html
qmake -r && make docs_online || exit 1
cd doc
cp -r html qt-creator-${VERSION}
zip -r ../qt-creator-${VERSION}-doc.zip qt-creator-${VERSION}
rm -r qt-creator-${VERSION}
