# Brand images

`logo.png` (here) and `../../packaging/icons/icon.png`, `largeIcon.png` are drawn by
`tools/make_brand.py` from CloudStream's launcher vector
`app/src/main/res/drawable-v24/ic_launcher_foreground.xml` (recloudstream/cloudstream at `eb962b4`,
GPL-3.0, the same licence as this app): the cloud's three paths and their gradients, unchanged.

The launcher icons add a "webOS" label so that this native webOS build can be told apart from
CloudStream's Android app. The label is the word set in Atkinson Hyperlegible Bold (OFL, the kit's
font), not LG's logo; webOS is a trademark of LG Electronics and is used only to name the platform.

```sh
python3 -I apps/csweb-tv/tools/make_brand.py \
    external/cloudstream/app/src/main/res/drawable-v24/ic_launcher_foreground.xml \
    external/webos-native-app-kit/assets/fonts/AtkinsonHyperlegible-Bold.ttf <out dir>
```
