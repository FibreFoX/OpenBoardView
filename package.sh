#!/bin/bash
VERS=$1
set -x

HD=`pwd`

export OBV_BUILD=`git rev-list HEAD --count`
export PP="OBV-R${OBV_BUILD}"
mkdir tmp

if [ "$VERS" == "win" ]; then
	
	export CROSS=mingw64
	./build.sh --recompile
	if [ $? -ne 0 ]; then
		echo "Windows OBV build failed"
		exit 1
	fi

	export PP="OBV-R${OBV_BUILD}-win"
	strip bin/openboardview.exe
	echo "Strip exit code = $?"

	mkdir wintmp
	mkdir wintmp/${PP}
	cp bin/openboardview.exe wintmp/${PP}
	cd wintmp/${PP}

	ln -s openboardview.exe obv-gl1.exe
	ln -s openboardview.exe obv-gl3.exe
	ln -s openboardview.exe obv-gles2.exe

	cd ..
	rm ../${PP}.zip
	zip -rv ../${PP}.zip ${PP}
	cd ..
fi

if [ "$VERS" == "lindeb" -o "$VERS" == "linrpm" -o "$VERS" == "linux" ]; then

	export CROSS=
	./build.sh --recompile
	if [ $? -ne 0 ]; then
		echo "Linux OBV build failed"
		exit 1
	fi

	export PP="OBV-R${OBV_BUILD}-linux"
	mkdir tmp/${PP}
	strip bin/openboardview
	cp bin/openboardview tmp/${PP}
	cd tmp
	tar zcvf ../${PP}.tar.gz ${PP}
	cd ..
fi

if [ "$VERS" == "macos" ]; then

	./build.sh --recompile
	if [ $? -ne 0 ]; then
		echo "macOS OBV build failed"
		exit 1
	fi
	
	export PP="OBV-R${OBV_BUILD}-macos"
	mkdir tmp/${PP}
	cp -rv openboardview.app tmp/${PP}
	cp $HD/asset/Retina-Info.plist tmp/${PP}/openboardview.app/Contents/Info.plist
	mkdir tmp/${PP}/openboardview.app/Contents/Resources
	cd tmp
	../fix-dylibs.sh ${PP}/openboardview.app
	../fix-dylibc.sh ${PP}/openboardview.app/Contents/MacOS/obvpdf.app
	zip -r ../${PP}.zip ${PP}
	cd ..
fi


