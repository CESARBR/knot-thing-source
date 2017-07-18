#!/bin/bash
read -p "Tag to apply: " TAG
sed -i "/version/ s/=.*/=$TAG/g" ./src/library.properties
sed -i '/version/ s/\([v\.]\)0\+\([[:digit:]]\+\)/\1\2/g' ./src/library.properties
sed -i '/version/ s/KNOT-v//g' ./src/library.properties
git add src/library.properties
git commit -m "Update to version $TAG"
git tag "$TAG"
