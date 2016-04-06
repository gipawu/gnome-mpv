gsettings_SCHEMAS = org.gnome.GnomeMpv.gschema.xml

@GSETTINGS_RULES@

appstream_in_files = org.gnome.GnomeMpv.appdata.xml.in
appstream_XML = $(appstream_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@
@APPSTREAM_XML_RULES@

desktopdir = $(datadir)/applications
desktop_in_files = org.gnome.GnomeMpv.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

icondir = $(datadir)/icons/hicolor/scalable/apps
icon_DATA = gnome-mpv.svg

symicondir = $(datadir)/icons/hicolor/symbolic/apps
symicon_DATA = gnome-mpv-symbolic.svg

EXTRA_DIST =	$(appstream_in_files) $(desktop_in_files) \
		gnome-mpv.svg gnome-mpv-symbolic.svg \
		org.gnome.GnomeMpv.gschema.xml.in \
		mpris_gdbus.xml marshal.lst

DISTCLEANFILES = $(appstream_XML) $(gsettings_SCHEMAS) $(desktop_DATA)
