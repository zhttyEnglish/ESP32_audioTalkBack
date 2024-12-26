# Changelog

## v2.0.0

### Features

- Add audio decoder common APIs to operate all supported decoders
- Add audio decoder implementation for `AAC`, `MP3`, `OPUS`, `ADPCM`, `G711A`, `G711U`, `AMRNB`, `AMRWB`, `VORBIS`, `ALAC`
- Add audio simple decoder common APIs to operate all supported simple decoders
- Add audio simple decoder implementation for `AAC`, `MP3`, `M4A`, `TS`, `AMRNB`, `AMRWB`, `FLAC`, `WAV`
- Add `esp_es_parse` to easily parse and get audio frame
- Add audio encoder registration and customization support through `esp_audio_enc_register`, deprecate `esp_audio_enc_install` and `esp_audio_enc_uninstall` APIs
- Add audio decoder registration and customization support through `esp_audio_dec_register`
- Add audio simple decoder registration and customization support through `esp_audio_simple_dec_register`
- Add audio encoder support for `ALAC`
- Refine memory usage when not enable AAC-Plus support
- Reorganized code layout, separate implementation from common part
- Add default registration for supported audio encoders (controlled by menuconfig) through `esp_audio_enc_register_default`
- Add default registration for supported audio decoders (controlled by menuconfig) through `esp_audio_dec_register_default`
- Add default registration for supported audio simple decoders (controlled by menuconfig) through `esp_audio_simple_dec_register_default`
- Add memory management through `media_lib_sal` and can be traced through [mem_trace](https://github.com/espressif/esp-adf-libs/tree/master/media_lib_sal/mem_trace)

### Bug Fixes

- Fix audio encoder PTS calculation error when run long time


## v1.0.1

### Bug Fixes

- Fix `CMakeLists.txt` hardcode on prebuilt library name
- Refine test code in [README.md](README.md)


## v1.0.0

### Features

- Initial version of `esp-audio-codec`
- Add audio encoder common part
- Add audio encoder implementation for `AAC`, `OPUS`, `ADPCM`, `G711A`, `G711U`, `AMRNB`, `AMRWB`, `PCM`
