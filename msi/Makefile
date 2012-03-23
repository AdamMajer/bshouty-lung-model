WIXDIR=C:\wix35

#LIBS=$(WIXDIR)\wixca.wixlib $(WIXDIR)\wixui.wixlib $(WIXDIR)\sca.wixlib
LIBS=

all: 
	echo "Specify target: bshouty_lung_model_64bit.msi | bshouty_lung_model_32bit.msi

bshouty_lung_model_64bit.msi: main64.wixobj ..\release\bshouty_model.exe
	$(WIXDIR)\light -ext WixUIExtension -ext WixUtilExtension -cultures:en-us -out bshouty_lung_model_64bit.msi main64.wixobj

bshouty_lung_model_32bit.msi: main32.wixobj 
	$(WIXDIR)\light -ext WixUIExtension -ext WixUtilExtension -cultures:en-us -out bshouty_lung_model_32bit.msi main32.wixobj

main64.wixobj: main64.wxs
	$(WIXDIR)\candle main64.wxs

main32.wixobj: main32.wxs
	$(WIXDIR)\candle main32.wxs

clean:
	del main.wixobj
	del bshouty_lung_model_64bit.msi
	del bshouty_lung_model_32bit.msi
