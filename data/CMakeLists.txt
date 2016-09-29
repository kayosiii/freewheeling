set (FWEELIN_XMLS basics.xml bcf2000.xml coreinterface.xml fweelin.xml interfaces.xml midifootswitch.xml patches1.xml patches3.xml patches-channels.xml pcr-m1.xml synthedit.xml
bcf2000-synthedit.xml browsers.xml dancemat.xml graphics.xml mercury.xml midikeyboard.xml patches2.xml patches-b3.xml pckeyboard.xml rme-hdsp.xml )
set (FWEELIN_TXTS bcf2000-help.txt config-help.txt)
set (FWEELIN_DATA ${FWEELIN_XMLS} ${FWEELIN_TXTS} basic.sf2 vera.ttf verabd.ttf)

install (FILES ${FWEELIN_DATA} DESTINATION share/fweelin)
