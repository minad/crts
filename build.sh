#!/bin/sh
mkdir coverity
cd coverity
wget https://scan.coverity.com/download/linux64 --quiet --post-data "token=$COVERITY_TOKEN&project=minad%2Fcrts" -O coverity_tool.tgz
if [ $? -ne 0 ]; then
  echo Download Coverity analysis tool failed!
  exit 1
fi
tar xzf cov*.tgz
rm -f cov*.tgz
export PATH="coverity/$(ls -d cov*)/bin:$PATH"
cd ..

cov-configure --compiler gcc-8 --comptype gcc --template
cov-build --dir cov-int make -j4

if [ $? -ne 0 ]; then
  cat cov-int/build-log.txt
  echo Build failed!
  exit 1
fi
cat cov-int/build-log.txt

tar czf crts.tgz cov-int
ls -l crts.tgz

curl --form token=$COVERITY_TOKEN \
  --form email=mail@daniel-mendler.de \
  --form file=@crts.tgz \
  --form version="master" \
  --form description="$(git log -1|head -1)" \
  https://scan.coverity.com/builds?project=minad%2Fcrts
