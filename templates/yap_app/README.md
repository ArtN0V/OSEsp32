# __APP_NAME__

This project was created from the OSEsp32 API 1.1 template.

From the OSEsp32 repository root on Windows PowerShell or Linux/macOS:

```text
python tools/yap.py check PATH_TO_THIS_PROJECT
python tools/yap.py build PATH_TO_THIS_PROJECT
```

Copy the generated `.yap` from this project's `build` directory to
`/OSEsp32/Apps` on the SD card. Edit `manifest.json` to request only the
capabilities the application actually uses. See `docs/YAP_API.md` for the API,
limits, errors and normal-exit rules.
