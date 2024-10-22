# File Transfer

[![Issues](https://img.shields.io/github/issues-raw/cnily03-hive/transf)](https://github.com/cnily03-hive/transf/issues)
[![Stars](https://img.shields.io/github/stars/cnily03-hive/transf)](https://github.com/cnily03-hive/transf/stargazers)
[![License](https://img.shields.io/github/license/cnily03-hive/transf)](https://github.com/cnily03-hive/transf?tab=MIT-1-ov-file)

This is the homework of chapter 6-7 of Computer Network Experiment 2024 in CUMT.

This repository implemented file transfer via UDP/TCP protocol, with security check and beautified output.

## Development

The compile environment is MinGW Clang Toolchian, posix thread, C++ 20 standard on Windows OS. Please make sure you have Windows Kits on your computer.

If you meet problems when compiling, give a try to [LLVM MinGW](https://github.com/mstorsjo/llvm-mingw/releases), or refer to [workflow file](./.github/workflows/compile.yml).

The program will save the received file to `received` directory by default, you can change it by specify `--dir` option.

For more information, please refer to the help message via `--help`.

```shell
Usage: transf_server [ip] <port> [options]

Options:
  -h, --help               Display this help message
  -d, --dir <path>         Save received files to the specified directory
  --protocol <protocol>    Specify the protocol to use (default: udp)
  --tcp                    Equivalent to --protocol tcp
  --udp                    Equivalent to --protocol udp
  --chunk <size>           Set chunk size for file transfer (default: 2048)
  --timeout <timeout>      Set timeout for sending and receiving data (default: 10000)
  --debug                  Enable debug mode
  --listen-all             Listen on all available interfaces

Copyright (c) 2024 Jevon Wang, MIT License
Source code: github.com/cnily03-hive/transf
```

```shell
Usage: transf_client <ip> <port> [options]

Options:
  -h, --help               Display this help message
  --ping                   Ping the server
  --protocol <protocol>    Specify the protocol to use (default: udp)
  --tcp                    Equivalent to --protocol tcp
  --udp                    Equivalent to --protocol udp
  --chunk <chunk_size>     Set chunk size for file transfer (default: 2048)
  --timeout <timeout>      Set timeout for sending and receiving data (default: 10000)
  --debug                  Enable debug mode

Copyright (c) 2024 Jevon Wang, MIT License
Source code: github.com/cnily03-hive/transf
```

> [!TIP]
> The source code doesn't examine the lock of the file system, and it will overwrite the file if the file exists. Please be careful when using the program, though it have been equipped with some security check and mutex control.

This program will be updated in the future, welcome to contribute.

## License

Copyright (c) Cnily03. All rights reserved.

Licensed under the [MIT](./LICENSE) License.
