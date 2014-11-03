#!/bin/bash

# Bash script that helps with releasing new versions of EasyLogging++
# Revision: 1.2
# @author mkhan3189
#
# Usage:
#        ./release.sh [repo-root] [homepage-repo-root] [new-version] [do-not-ask]

if [ "$1" = "" ];then
  echo
  echo "Usage: $0 [repository-root] [homepage-root] [new-version] [do-not-ask]"
  echo
  exit 1
fi

if [ -f "$1/tools/release.sh" ];then
  [ -d "$2/releases/" ] || mkdir $2/releases/
else
  echo "Invalid repository root"
  exit 1
fi

CURR_VERSION=$(grep 'Easylogging++ v' $1/src/easylogging++.h | grep -o '[0-9].[0-9][0-9]*')
CURR_RELEASE_DATE=$(grep -o '[0-9][0-9]-[0-9][0-9]-201[2-9] [0-9][0-9][0-9][0-9]hrs' $1/src/easylogging++.h)
NEW_RELEASE_DATE=$(date +"%d-%m-%Y %H%Mhrs")
NEW_VERSION=$3
DO_NOT_CONFIRM=$4
if [ "$NEW_VERSION" = "" ]; then
  echo 'Current Version  ' $CURR_VERSION
  echo '** No version provided **'
  exit
fi

echo 'Current Version  ' $CURR_VERSION ' (' $CURR_RELEASE_DATE ')'
echo 'New Version      ' $NEW_VERSION  ' (' $NEW_RELEASE_DATE ')'
if [ "$DO_NOT_CONFIRM" = "y" ]; then
  confirm="y"
else
  echo 'Are you sure you wish to release new version? (y/n)' 
  read confirm
fi

if [ "$confirm" = "y" ]; then
  sed -i "s/Easylogging++ v$CURR_VERSION*/Easylogging++ v$NEW_VERSION/g" $1/src/easylogging++.h
  sed -i "s/Easylogging++ v$CURR_VERSION*/Easylogging++ v$NEW_VERSION/g" $1/README.md
  sed -i "s/version(void) { return std::string(\"$CURR_VERSION\"); }/version(void) { return std\:\:string(\"$NEW_VERSION\"); }/g" $1/src/easylogging++.h
  sed -i "s/releaseDate(void) { return std::string(\"$CURR_RELEASE_DATE\"); }/releaseDate(void) { return std\:\:string(\"$NEW_RELEASE_DATE\"); }/g" $1/src/easylogging++.h
  sed -i "s/ (development \/ unreleased version)//g" $1/src/easylogging++.h
  sed -i "s/\$currentVersion = \"$CURR_VERSION\"*/\$currentVersion = \"$NEW_VERSION\"/g" $2/version.php
  sed -i "s/\$releaseDate = \"$CURR_RELEASE_DATE\"*/\$releaseDate = \"$NEW_RELEASE_DATE\"/g" $2/version.php
  sed -i "s/$CURR_RELEASE_DATE/$NEW_RELEASE_DATE/g" $2/version.php
  sed -i "s/v$CURR_VERSION/v$NEW_VERSION/g" $1/README.md
  sed -i "s/easyloggingpp\/blob\/v$CURR_VERSION\/README.md/easyloggingpp\/blob\/v$NEW_VERSION\/README.md/g" $1/doc/RELEASE-NOTES-v$NEW_VERSION
  sed -i "s/easyloggingpp_$CURR_VERSION.zip/easyloggingpp_$NEW_VERSION.zip/g" $1/README.md
  if [ -f "easyloggingpp_v$NEW_VERSION.zip" ]; then
    rm easyloggingpp_v$NEW_VERSION.zip
  fi
  if [ -f "easyloggingpp.zip" ]; then
    rm easyloggingpp.zip
  fi
  cp $1/src/easylogging++.h .
  zip easyloggingpp_v$NEW_VERSION.zip easylogging++.h
  tar -pczf easyloggingpp_v$NEW_VERSION.tar.gz easylogging++.h
  zip latest.zip easylogging++.h
  mv latest.zip $2/
  mv easyloggingpp_v$NEW_VERSION.zip $2/releases/
  mv easyloggingpp_v$NEW_VERSION.tar.gz $2/releases/
  cp $1/doc/RELEASE-NOTES-v$NEW_VERSION $2/release-notes-latest.txt
  cp $1/doc/RELEASE-NOTES-v$NEW_VERSION $2/releases/release-notes-v$NEW_VERSION.txt
  rm easylogging++.h
fi
