# IndoorCombo

## Introduction

The project provides examples of how to develop project for IndorCombo. It is based on the [ESP-IDF](https://github.com/espressif/esp-idf).


## Getting Started

### Install ESP IDF

Follow instructions in this guide
[ESP-IDF - Get Started](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)
to setup the built toolchain used by SSCMA examples. Currently we're using the latest version `v5.1`.

### Clone and Setup the Repository

1. Clone our repository.

    ```sh
    git clone https://github.com/Seeed-Studio/IndoorCombo
    ```

2. Go to `IndoorCombo` folder.

    ```sh
    cd IndoorCombo
    ```

3. Fetch the submodules.

    ```sh
    git submodule update --init
    ```

### Build and Run Examples

1. Go to examples folder and list all available examples.

    ```sh
    cd examples && \
    ls
    ```

2. Choose a `<demo>` and enter its folder.

    ```sh
    cd '<demo>'
    ```

3. Generate build config using ESP-IDF.

    ```sh
    # set build target
    idf.py set-target esp32s3
    ```

4. Build the demo firmware.

    ```sh
    idf.py build
    ```

5. Flash the demo firmware to device and Run.

    To flash (the target serial port may vary depend on your operating system, please replace `/dev/ttyACM0` with your device serial port).

    ```
    idf.py --port /dev/ttyACM0 flash
    ```

    Monitor the serial output.

    ```
    idf.py --port /dev/ttyACM0 monitor
    ```

#### Tip

- Use `Ctrl+]` to exit monitor.

- The previous two commands can be combined.

    ```sh
    idf.py --port /dev/ttyACM0 flash monitor
    ```




## Contributing

- If you find any issue in using these examples, or wish to submit an enhancement request, please use the raise a [Issue](https://github.com/Seeed-Studio/IndoorCombo/issues) or submit a [Pull Request](https://github.com/Seeed-Studio/IndoorCombo/pulls).


## License

```
These examples are covered under MIT License.
